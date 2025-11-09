#include "stdio.h"
#include "assert.h"
#include "stdlib.h"
#include "tree.h"



/* need exclusive rights on tree before call */
struct tree_allocate_version_result
{
    int64_t version_id;
    struct tree_version *version;
};
static struct tree_allocate_version_result tree_allocate_version(struct tree *tree, int64_t parent)
{    
    int64_t new_version_id = tree->versions_len++;
    
    if (new_version_id > tree->versions_alloc)
    {
        tree->versions_alloc = 2 * tree->versions_alloc + !tree->versions_alloc;
        void *new_ptr = realloc(tree->versions, sizeof(*tree->versions) * tree->versions_alloc);
        if (new_ptr == NULL)
        {
            return (struct tree_allocate_version_result){-1, NULL};
        }
        tree->versions = new_ptr;
    }

    struct tree_version *version = tree->versions[new_version_id] = calloc(1, sizeof(*version));
    
    version->parent = parent;
    version->lock = (SRWLOCK)SRWLOCK_INIT;

    AcquireSRWLockExclusive(&version->lock);

    if (parent == new_version_id)
    {
        version->root = parent;
        version->size = 0;
        version->far_parent = parent;
        version->depth = 0;
    }
    else
    {
        version->root = tree->versions[parent]->root;
        version->size = tree->versions[parent]->size;
        
        if (tree->versions[parent]->depth
             - tree->versions[tree->versions[parent]->far_parent]->depth ==
            tree->versions[tree->versions[parent]->far_parent]->depth
             - tree->versions[tree->versions[tree->versions[parent]->far_parent]->far_parent]->depth)
        {
            version->far_parent = tree->versions[tree->versions[parent]->far_parent]->far_parent;
        }
        else
        {
            version->far_parent = parent;
        }
        version->depth = tree->versions[parent]->depth + 1;
    }
    
    return (struct tree_allocate_version_result){new_version_id, version};
}


struct tree *tree_create()
{
    struct tree *tree = calloc(1, sizeof(*tree));

    tree->versions_len = 0;
    tree->versions_alloc = 128;
    tree->versions = calloc(1, sizeof(*tree->versions) * tree->versions_alloc);

    tree->lock = (SRWLOCK)SRWLOCK_INIT;

    /* create allocator */
    tree->allocator = allocator_create();

    /* initializate root version */
    struct tree_allocate_version_result result = tree_allocate_version(tree, 0);
    assert(result.version_id == 0);
    assert(result.version != NULL);
    
    return tree;
}

void tree_free(struct tree *tree)
{
    AcquireSRWLockExclusive(&tree->lock);
    
    /* free all nodes? */
    allocator_free(tree->allocator);
    
    /* free tree */
    free(tree);
}

struct tree_split_node_result tree_split_node(struct tree *tree, int64_t version, int64_t node_id, int32_t node_is_now_left, struct question *question)
{
    /* copy tree state */
    AcquireSRWLockExclusive(&tree->lock);
    struct tree_allocate_version_result new_version = tree_allocate_version(tree, version);
    ReleaseSRWLockExclusive(&tree->lock);

    if (new_version.version_id == -1 || new_version.version == NULL)
    {
        return (struct tree_split_node_result){-1, INVALID_NODE_ID};
    }
    
    /* result version have exclusive access */
    /* create new node in tree: */
    struct allocator_create_node_result new_node = allocator_create_node(tree->allocator, NODE_VARIANT);

    if (new_node.node == NULL || new_node.node_id == INVALID_NODE_ID)
    {
        return (struct tree_split_node_result){-1, INVALID_NODE_ID};
    }

    if (node_id == INVALID_NODE_ID)
    {
        new_node.node->parent = INVALID_NODE_ID;
    }
    else
    {
        struct node *node = allocator_acquire_node(tree->allocator, node_id, 0);
        new_node.node->parent = node->parent;
        allocator_release_node(tree->allocator, node_id, 0);
    }

    /* fill node by given data */
    ((struct node_variant *)new_node.node)->question = question;
    if (node_is_now_left)
    {
        ((struct node_variant *)new_node.node)->l = node_id;
        ((struct node_variant *)new_node.node)->r = INVALID_NODE_ID;
    }
    else
    {
        ((struct node_variant *)new_node.node)->l = INVALID_NODE_ID;
        ((struct node_variant *)new_node.node)->r = node_id;
    }
    
    
    /* create copy of node, and all it's parents -> create new root */
    int64_t prev_node_id = node_id;
    int64_t new_root_id = new_node.node_id;
    struct node *new_root = new_node.node;
    
    assert(new_root_id != INVALID_NODE_ID);
    
    while (new_root->parent != INVALID_NODE_ID)
    {
        /* get parent of new_root_id */
        int64_t next_root_id = new_root->parent;
        struct allocator_create_node_result next_root_copy;
        {
            struct node *next_root = allocator_acquire_node(tree->allocator, next_root_id, 0);
            if (next_root == NULL)
            {
                return (struct tree_split_node_result){-1, INVALID_NODE_ID};
            }

            /* create copy of this node */
            next_root_copy = allocator_create_node(tree->allocator, NODE_VARIANT);
            node_copy(next_root_copy.node, next_root);

            allocator_release_node(tree->allocator, next_root_id, 0);
        }

        /* set new_root as it's child */
        {
            struct node_variant *next_root_copy_var = (struct node_variant *)next_root_copy.node;

            assert(!!(next_root_copy_var->l == prev_node_id) + 
                   !!(next_root_copy_var->r == prev_node_id) == 1);
            
            if (next_root_copy_var->l == prev_node_id)
            {
                next_root_copy_var->l = new_root_id;
            }
            if (next_root_copy_var->r == prev_node_id)
            {
                next_root_copy_var->r = new_root_id;
            }
        }

        /* release this copy node */
        ReleaseSRWLockExclusive(&new_root->lock);
        
        /* process next node */
        prev_node_id = next_root_id;
        new_root_id = next_root_copy.node_id;
        new_root = next_root_copy.node;
    }

    /* now: - new_root is in exclusive access */
    /*      - new_root->parent == INVALID_NODE_ID */
    /* 1. set it as new root */
    new_version.version->root = new_root_id;
    new_version.version->size++;

    /* 2. release new_root node */
    ReleaseSRWLockExclusive(&new_root->lock);    
    ReleaseSRWLockExclusive(&new_version.version->lock);    
    
    return (struct tree_split_node_result){new_version.version_id, new_node.node_id};
}

struct tree_set_leaf_result tree_set_leaf(struct tree *tree, int64_t version, int64_t node_id, struct record *record)
{
    
}
