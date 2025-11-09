#include "tree.h"
#include "malloc.h"

struct node *node_create(enum node_type type)
{
    switch (type)
    {
        case NODE_VARIANT:
        {
            struct node_variant *node = calloc(1, sizeof(*node));

            /* base class */
            node->type = type;
            node->ptr_count = 1;
            node->last_access = 0;
            node->lock = (SRWLOCK)SRWLOCK_INIT;

            node->l = INVALID_NODE_ID;
            node->r = INVALID_NODE_ID;
            node->question = NULL;

            AcquireSRWLockExclusive(&node->lock);

            return (struct node *)node;
        }
        case NODE_LEAF:
        {
            struct node_leaf *node = calloc(1, sizeof(*node));

            /* base class */
            node->type = type;
            node->ptr_count = 1;
            node->last_access = 0;
            node->lock = (SRWLOCK)SRWLOCK_INIT;

            node->guess = NULL;
            
            AcquireSRWLockExclusive(&node->lock);
            
            return (struct node *)node;
        }
        default:
            return NULL;
    }
}
