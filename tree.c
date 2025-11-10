#define __USE_MINGW_ANSI_STDIO 1
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
        version->root = INVALID_NODE_ID;
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
    tree->allocator = allocator_create("./akinator.db");

    /* initializate root version */
    struct tree_allocate_version_result result = tree_allocate_version(tree, 0);
    assert(result.version_id == 0);
    assert(result.version != NULL);

    /* release root version */
    ReleaseSRWLockExclusive(&result.version->lock);
    
    return tree;
}

void tree_sync_and_free(struct tree *tree)
{
    AcquireSRWLockExclusive(&tree->lock);
    
    /* free all nodes? */
    allocator_sync_and_free(tree->allocator);
    
    /* free tree */
    free(tree);
}

void tree_free(struct tree *tree)
{
    AcquireSRWLockExclusive(&tree->lock);
    
    /* free all nodes? */
    allocator_free(tree->allocator);
    
    /* free tree */
    free(tree);
}


struct node_allocator *tree_get_allocator(struct tree *tree)
{
    AcquireSRWLockShared(&tree->lock);
    struct node_allocator *allocator = tree->allocator;
    ReleaseSRWLockShared(&tree->lock);
    return allocator;
}


struct tree_version *tree_acquire_version(struct tree *tree, int64_t version_id, int32_t exclusive)
{
    AcquireSRWLockShared(&tree->lock);
    struct tree_version *version = tree->versions[version_id];
    if (exclusive)
    {
        AcquireSRWLockExclusive(&version->lock);
    }
    else
    {
        AcquireSRWLockShared(&version->lock);
    }
    ReleaseSRWLockShared(&tree->lock);
    return version;
}


void tree_release_version_by_id(struct tree *tree, int64_t version_id, int32_t exclusive)
{
    AcquireSRWLockShared(&tree->lock);
    
    assert(version_id < tree->versions_len);
    
    struct tree_version *version = tree->versions[version_id];
    tree_release_version(tree, version, exclusive);
    
    ReleaseSRWLockShared(&tree->lock);
}

void tree_release_version(struct tree *tree, struct tree_version *version, int32_t exclusive)
{
    (void)tree;
    
    assert(version != NULL);
    
    if (exclusive)
    {
        ReleaseSRWLockExclusive(&version->lock);
    }
    else
    {
        ReleaseSRWLockShared(&version->lock);
    } 
}

/* !iterator must point on prev_node_id node! */
static int32_t tree_copy_and_set_new_root(struct tree *tree, 
                                int64_t new_version_id, 
                                struct tree_version *new_version,
                                struct tree_iterator *iterator,
                                int64_t new_node_id,
                                struct node *new_node)
{    
    (void)new_version_id;

    for (int64_t depth = iterator->path_len - 1; depth > 0; --depth)
    {
        int64_t current_id = iterator->path[depth];
        // printf("we have copy of %lld\n", current_id);

        /* get it's parent */
        int64_t parent_id = iterator->path[depth - 1];
        
        // printf("we need to create copy of %lld\n", parent_id);
        
        assert(current_id != INVALID_NODE_ID);
        assert(parent_id != INVALID_NODE_ID);
        
        /* create copy of this parent */
        struct allocator_create_node_result parent_copy;
        {
            struct node *parent = allocator_acquire_node(tree->allocator, parent_id, 0);
            if (parent == NULL)
            {
                return -1;
            }

            /* create copy of this node */
            parent_copy = allocator_create_node(tree->allocator, NODE_VARIANT);
            node_make_copy(parent_copy.node, parent);

            allocator_release_node(tree->allocator, parent, 0);
        }

        /* redirect it's child on new node */
        // printf("created copy: %lld node!\n", parent_copy.node_id);
        // printf("set one of it's child on %lld node!\n", new_node_id);

        /* set new_node_to_insert as it's child */
        {
            struct node_variant *parent_copy_var = (struct node_variant *)parent_copy.node;

            assert(!!(parent_copy_var->l == current_id) + 
                   !!(parent_copy_var->r == current_id) == 1);
            if (parent_copy_var->l == current_id)
            {
                parent_copy_var->l = new_node_id;
            }
            if (parent_copy_var->r == current_id)
            {
                parent_copy_var->r = new_node_id;
            }
        }
        
        /* release current node */
        ReleaseSRWLockExclusive(&new_node->lock);
        
        /* process to next node */
        new_node_id = parent_copy.node_id;
        new_node = parent_copy.node;
    }

    /* now: - new_node_to_insert is in exclusive access */
    /*      - new_node_to_insert->parent == INVALID_NODE_ID */
    /* 1. set it as new root */
    new_version->root = new_node_id;
    new_version->size++;

    /* 2. release new_node_to_insert node */
    ReleaseSRWLockExclusive(&new_node->lock);

    return 0; 
}

struct tree_split_node_result tree_split_node(struct tree *tree, struct tree_iterator *iterator, int32_t node_is_now_left, struct question *question)
{
    /* copy tree state */
    AcquireSRWLockExclusive(&tree->lock);
    struct tree_allocate_version_result new_version = tree_allocate_version(tree, iterator->version_id);
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
        ReleaseSRWLockExclusive(&new_version.version->lock);
        return (struct tree_split_node_result){-1, INVALID_NODE_ID};
    }

    int64_t node_id = tree_iterator_get_node(iterator, 0);

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

    if (tree_copy_and_set_new_root(tree, new_version.version_id, new_version.version, iterator, new_node.node_id, new_node.node) != 0)
    {
        ReleaseSRWLockExclusive(&new_version.version->lock);
        return (struct tree_split_node_result){-1, INVALID_NODE_ID};
    }
    
    ReleaseSRWLockExclusive(&new_version.version->lock);
    
    /* create copy of node, and all it's parents -> create new root */
    return (struct tree_split_node_result){new_version.version_id, new_node.node_id};
}


struct tree_set_leaf_result tree_set_leaf(struct tree *tree, struct tree_iterator *iterator, int32_t set_as_left_child, struct record *record)
{
    /* copy tree state */
    AcquireSRWLockExclusive(&tree->lock);
    struct tree_allocate_version_result new_version = tree_allocate_version(tree, iterator->version_id);
    ReleaseSRWLockExclusive(&tree->lock);

    if (new_version.version_id == -1 || new_version.version == NULL)
    {
        return (struct tree_set_leaf_result){-1, INVALID_NODE_ID};
    }
    
    /* result version have exclusive access */
    /* create new node in tree: */
    struct allocator_create_node_result new_node = allocator_create_node(tree->allocator, NODE_LEAF);

    if (new_node.node == NULL || new_node.node_id == INVALID_NODE_ID)
    {
        ReleaseSRWLockExclusive(&new_version.version->lock);
        return (struct tree_set_leaf_result){-1, INVALID_NODE_ID};
    }

    /* fill node by given data */
    ((struct node_leaf *)new_node.node)->record = record;

    /* release created node */
    ReleaseSRWLockExclusive(&new_node.node->lock);
    
    int64_t node_id = tree_iterator_get_node(iterator, 0);

    if (node_id == INVALID_NODE_ID)
    {
        assert(new_version.version->root == INVALID_NODE_ID);
        new_version.version->root = new_node.node_id;
    }
    else
    {
        /* create parent (=node) copy */
        struct node *node = allocator_acquire_node(tree->allocator, node_id, 0);
        if (node == NULL)
        {
            ReleaseSRWLockExclusive(&new_version.version->lock);
            return (struct tree_set_leaf_result){-1, INVALID_NODE_ID};
        }

        /* can't add child to NODE_LEAF */
        assert(node->type == NODE_VARIANT);
        
        /* create copy */
        struct allocator_create_node_result node_copy = allocator_create_node(tree->allocator, NODE_VARIANT);
        node_make_copy(node_copy.node, node);

        allocator_release_node(tree->allocator, node, 0);
        
        if (set_as_left_child)
        {
            assert(((struct node_variant *)node_copy.node)->l == INVALID_NODE_ID);
            ((struct node_variant *)node_copy.node)->l = new_node.node_id;
        }
        else
        {
            assert(((struct node_variant *)node_copy.node)->r == INVALID_NODE_ID);
            ((struct node_variant *)node_copy.node)->r = new_node.node_id;
        }
        
        if (tree_copy_and_set_new_root(tree, new_version.version_id, new_version.version, iterator, node_copy.node_id, node_copy.node) != 0)
        {
            ReleaseSRWLockExclusive(&new_version.version->lock);
            return (struct tree_set_leaf_result){-1, INVALID_NODE_ID};
        }
    }
    
    ReleaseSRWLockExclusive(&new_version.version->lock);
    
    /* create copy of node, and all it's parents -> create new root */
    return (struct tree_set_leaf_result){new_version.version_id, new_node.node_id};
}
