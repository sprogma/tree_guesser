#include "tree.h"
#include "stdio.h"
#include "assert.h"

static void iterator_add_node(struct tree_iterator *iterator, int64_t node_id)
{
    if (iterator->path_len >= iterator->path_alloc)
    {
        iterator->path_alloc = 2 * iterator->path_alloc + !iterator->path_alloc;
        void *new_ptr = realloc(iterator->path, sizeof(*iterator->path) * iterator->path_alloc);
        if (new_ptr == NULL)
        {
            // TODO: error report
            return;
        }
        iterator->path = new_ptr;
    }
    
    iterator->path[iterator->path_len++] = node_id;
}

struct tree_iterator *tree_iterator_create(struct tree *tree, int64_t version_id)
{
    struct tree_iterator *iterator = calloc(1, sizeof(*iterator));
    
    iterator->tree = tree;
    iterator->version_id = version_id;
    iterator->path_len = 0;
    iterator->path_alloc = 16;
    iterator->path = calloc(1, sizeof(*iterator->path) * iterator->path_alloc);

    /* try to select root */
    tree_iterator_move_down(iterator, 0);
    
    return iterator;
}

int64_t tree_iterator_get_node(struct tree_iterator *iterator, int64_t shift_from_current)
{
    if (shift_from_current >= iterator->path_len)
    {
        return INVALID_NODE_ID;
    }
    return iterator->path[iterator->path_len - 1 - shift_from_current];
}

int64_t tree_iterator_get_depth(struct tree_iterator *iterator)
{
    return iterator->path_len - 1;
}

void tree_iterator_move_up(struct tree_iterator *iterator)
{
    assert(iterator->path_len != 0);
    iterator->path_len--;
}

void tree_iterator_move_down(struct tree_iterator *iterator, int32_t move_to_left)
{
    if (iterator->path_len == 0)
    {
        struct tree_version *version = tree_acquire_version(iterator->tree, iterator->version_id, 0);

        if (version->root != INVALID_NODE_ID)
        {
            iterator_add_node(iterator, version->root);
        }
        
        tree_release_version(iterator->tree, version, 0);
    }
    else
    {
        struct node_allocator *allocator = tree_get_allocator(iterator->tree);
        
        struct node *node = allocator_acquire_node(allocator, iterator->path[iterator->path_len - 1], 0);
        
        assert(node->type == NODE_VARIANT);
        
        struct node_variant *variant = (struct node_variant *)node;
        
        int64_t next_node_id = INVALID_NODE_ID;
        if (move_to_left)
        {
            next_node_id = variant->l;
        }
        else
        {
            next_node_id = variant->r;
        }
        
        if (next_node_id == INVALID_NODE_ID)
        {
            iterator->path_len = 0;
        }
        else
        {
            iterator_add_node(iterator, next_node_id);
        }
        
        allocator_release_node(allocator, node, 0);
    }
}

void tree_iterator_free(struct tree_iterator *iterator)
{
    /* free all structures */
    free(iterator->path);
    free(iterator);
}

struct tree_iterator *tree_iterator_copy_rebase(struct tree_iterator *iterator, int64_t new_version_id)
{
    /* not implemented */
    return NULL;
    
    struct tree_iterator *result = tree_iterator_create(iterator->tree, new_version_id);
    
    struct tree_version *version = tree_acquire_version(iterator->tree, new_version_id, 0);

    /* if this is empty iterator - make that iterator empty too */
    if (iterator->path_len == 0)
    {
        tree_iterator_move_up(result);
        tree_release_version(iterator->tree, version, 0);
        return result;
    }
    else
    {
        if (tree_iterator_get_depth(result) == 0)
        {
            tree_release_version(iterator->tree, version, 0);
            tree_iterator_free(result);
            return NULL;
        }
        else
        {
            /* try to copy iterator's path */
            for (int i = 1; i < iterator->path_len; ++i)
            {
                // if (node_)
                // {
                //     
                // }
            }
        }
    }

    tree_release_version(iterator->tree, version, 0);
}
