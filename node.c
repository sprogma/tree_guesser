#include "tree.h"
#include "assert.h"
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

            node->record = NULL;
            
            AcquireSRWLockExclusive(&node->lock);
            
            return (struct node *)node;
        }
        default:
            return NULL;
    }
}


/* copy only copiable data [for example not copy ptr_count or last_access value] */
void node_make_copy(struct node *dest_bs, struct node *node_bs)
{
    assert(dest_bs->type == node_bs->type);
    
    switch (node_bs->type)
    {
        case NODE_VARIANT:
        {
            struct node_variant *dest = (struct node_variant *)dest_bs;
            struct node_variant *node = (struct node_variant *)node_bs;
            
            dest->l = node->l;
            dest->r = node->r;
            dest->question = node->question;
            return;
        }
        case NODE_LEAF:
        {
            struct node_leaf *dest = (struct node_leaf *)dest_bs;
            struct node_leaf *node = (struct node_leaf *)node_bs;
            
            dest->record = node->record;
            return;
        }
        default:
            return;
    }
}
