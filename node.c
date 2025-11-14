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
            node->question.text[0] = 0;

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

            node->record.name[0] = 0;
            
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


int32_t node_is_isomorphic(struct node *node_a, struct node *node_b)
{
    if (node_a->type != node_b->type)
    {
        return 0;
    }
    
    switch (node_a->type)
    {
        case NODE_VARIANT:
        {
            struct node_variant *node_a_var = (struct node_variant *)node_a;
            struct node_variant *node_b_var = (struct node_variant *)node_b;

            return strcmp(node_a_var->question.text, node_b_var->question.text) == 0;
        }
        case NODE_LEAF:
        {
            struct node_leaf *node_a_leaf = (struct node_leaf *)node_a;
            struct node_leaf *node_b_leaf = (struct node_leaf *)node_b;
            
            return strcmp(node_a_leaf->record.name, node_b_leaf->record.name) == 0;
        }
        default:
            return 0;
    }
}


int64_t node_get_size(struct node *node)
{
    switch (node->type)
    {
        case NODE_VARIANT:
        {
            return sizeof(struct node_variant);
        }
        case NODE_LEAF:
        {
            return sizeof(struct node_leaf);
        }
        default:
            return 0;
    }
}
