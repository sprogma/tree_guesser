#include "stdio.h"
#include "assert.h"
#include "stdlib.h"
#include "tree.h"



static int64_t tree_allocate_version(struct tree *tree, int64_t parent)
{
    int64_t new_version = tree->versions_len++;
    
    if (new_version > tree->versions_alloc)
    {
        tree->versions_alloc = 2 * tree->versions_alloc + !tree->versions_alloc;
        void *new_ptr = realloc(tree->versions, sizeof(*tree->versions) * tree->versions_alloc);
        if (new_ptr == NULL)
        {
            return -1;
        }
        tree->versions = new_ptr;
    }

    if (parent == new_version)
    {
        tree->versions[new_version].root = NULL;
        tree->versions[new_version].size = 0;
        tree->versions[new_version].parent = parent;
        tree->versions[new_version].far_parent = parent;
        tree->versions[new_version].depth = 0;
    }
    else
    {
        tree->versions[new_version].root = tree->versions[parent].root;
        tree->versions[new_version].size = tree->versions[parent].size;
        tree->versions[new_version].parent = parent;
        
        if (tree->versions[parent].depth
             - tree->versions[tree->versions[parent].far_parent].depth ==
            tree->versions[tree->versions[parent].far_parent].depth
             - tree->versions[tree->versions[tree->versions[parent].far_parent].far_parent].depth)
        {
            tree->versions[new_version].far_parent = tree->versions[tree->versions[parent].far_parent].far_parent;
        }
        else
        {
            tree->versions[new_version].far_parent = parent;
        }
        tree->versions[new_version].depth = tree->versions[parent].depth + 1;
    }
    
    return new_version;
}


struct tree *tree_create()
{
    struct tree *tree = calloc(1, sizeof(*tree));

    tree->versions_len = 0;
    tree->versions_alloc = 128;
    tree->versions = calloc(1, sizeof(*tree->versions) * tree->versions_alloc);

    tree->lock = (SRWLOCK)SRWLOCK_INIT;

    /* initializate root version */
    int64_t root = tree_allocate_version(tree, 0);
    assert(root == 0);

    /* create allocator */
    tree->allocator = allocator_create();
    
    return tree;
}

void tree_free(struct tree *tree)
{
    /* free all nodes? */
    allocator_free(tree->allocator);
    
    /* free tree */
    free(tree);
}
