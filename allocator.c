#include "stdlib.h"
#include "assert.h"
#include "stdio.h"
#include "tree.h"

struct node_allocator *allocator_create()
{
    struct node_allocator *allocator = calloc(1, sizeof(*allocator));
    
    allocator->nodes_alloc = 1024;
    allocator->nodes = calloc(1, sizeof(*allocator->nodes) * allocator->nodes_alloc);
    allocator->lock = (SRWLOCK)SRWLOCK_INIT;
    allocator->loaded_nodes = 0;
    
    AcquireSRWLockExclusive(&allocator->lock);
    
    /* start optimization process */
    // TODO:
    
    ReleaseSRWLockExclusive(&allocator->lock);
    
    return allocator;
}


void allocator_free(struct node_allocator *allocator)
{
    /* lock */
    AcquireSRWLockExclusive(&allocator->lock);
    /* stop all other processes? */
    // TODO:
    
    
    /* free allocator */
    for (int i = 0; i < allocator->nodes_len; ++i)
    {
        if (allocator->nodes[i] == UNLOADED_NODE)
        {
            fprintf(stderr, "Not implemented: unloading nodes\n");
            *(int *)NULL = 1;
            // TODO:
        }
        else
        {
            free(allocator->nodes[i]);
        }
    }
    free(allocator);
    
    // no release, becouse alloactor was deleted.
    // ReleaseSRWLockExclusive(&allocator->lock);
}


static void allocator_initializate_node(struct node_allocator *allocator, int64_t node_id, enum node_type type)
{
    /* !!! at begining of this function, allocator->lock must be 
           locked with exclusive access !!! */
   
    allocator->nodes[node_id] = node_create(type);

    int64_t access_token = allocator->access_token++;
    allocator->nodes[node_id]->last_access = access_token;

    /* ! not free node lock - it continue to be locked with exclusive access */
}


struct allocator_create_node_result allocator_create_node(struct node_allocator *allocator, enum node_type type)
{
    struct node *result_node;
    
    AcquireSRWLockShared(&allocator->lock);
    /* allocate new node */
    for (int i = 0; i < allocator->nodes_alloc; ++i)
    {
        if (allocator->nodes[i] == NULL)
        {
            /* try to insert node */
            /* change access on exclusive */
            ReleaseSRWLockShared(&allocator->lock);
            AcquireSRWLockExclusive(&allocator->lock);
            
            /* node could be allocated while changing access,
               so check that it is still free */
            if (allocator->nodes[i] == NULL)
            {
                allocator_initializate_node(allocator, i, type);

                result_node = allocator->nodes[i];
                
                ReleaseSRWLockExclusive(&allocator->lock);
                
                return (struct allocator_create_node_result){i, result_node};
            }
            
            /* else - continue searching */
            ReleaseSRWLockExclusive(&allocator->lock);
            AcquireSRWLockShared(&allocator->lock);
        }
    }
    ReleaseSRWLockShared(&allocator->lock);
    /* here - almost all table was used (may be not all)
       so reallocate node array */
    AcquireSRWLockExclusive(&allocator->lock);
    
    /* realloc */
    int64_t old_size = allocator->nodes_alloc;
    
    allocator->nodes_alloc = 2 * allocator->nodes_alloc + !allocator->nodes_alloc;
    void *new_ptr = realloc(allocator->nodes, sizeof(*allocator->nodes) * allocator->nodes_alloc);
    if (new_ptr == NULL)
    {
        /* can't insert new node */
        ReleaseSRWLockExclusive(&allocator->lock);
        return (struct allocator_create_node_result){INVALID_NODE_ID, NULL};
    }
    
    /* set all new nodes as not allocated */
    memset(allocator->nodes + old_size, 0, sizeof(*allocator->nodes) * (allocator->nodes_alloc - old_size));
    
    /* now, state is exclusive, so use it to insert new node */
    int64_t result_node_id = old_size;
    
    allocator_initializate_node(allocator, result_node_id, type);
    
    if (allocator->nodes[result_node_id] == NULL)
    {
        result_node_id = INVALID_NODE_ID;
        result_node = NULL;
    }
    else
    {
        result_node = allocator->nodes[result_node_id];
    }
    
    
    ReleaseSRWLockExclusive(&allocator->lock);
    
    return (struct allocator_create_node_result){result_node_id, result_node};
}


static void allocator_load_node(struct node_allocator *allocator, int64_t node_id, int32_t exclusive)
{
    fprintf(stderr, "Not implemented: loading/unloading nodes\n");
    *(int *)NULL = 1;
    // TODO:
}


static void allocator_unload_node(struct node_allocator *allocator, int64_t node_id)
{
    fprintf(stderr, "Not implemented: loading/unloading nodes\n");
    *(int *)NULL = 1;
    // TODO:
}


struct node *allocator_acquire_node(struct node_allocator *allocator, int64_t node_id, int32_t exclusive)
{
    assert(node_id != INVALID_NODE_ID);
    
    AcquireSRWLockShared(&allocator->lock);
    
    /* if node was unloaded */
    struct node *res = allocator->nodes[node_id];
    if (res == NULL)
    {
        /* not allocated node */
        return NULL;
    }
    else if (res == UNLOADED_NODE)
    {
        /* release allocator, load node, and acquire it back
           (becouse load_node will acquire exclusive access) */
        ReleaseSRWLockShared(&allocator->lock);
        
        /* load_node also will acquire node, becouse in another
           way, node can be unloaded before next lock. */
        allocator_load_node(allocator, node_id, exclusive);
        
        AcquireSRWLockShared(&allocator->lock);
        
        
        /* check - is node loaded or freed? 
           (if node was freed before loading) */
        res = allocator->nodes[node_id];
        assert(res != UNLOADED_NODE);
        if (res == NULL)
        {
            return NULL;
        }
        /* - node is loaded */
    }
    else
    {
        /* need to acquire node */
        if (exclusive)
        {
            AcquireSRWLockExclusive(&res->lock);
        }
        else
        {
            AcquireSRWLockShared(&res->lock);
        }
    }
    /* update last access token */
    int64_t access_token = allocator->access_token++;
    res->last_access = access_token;
        
    ReleaseSRWLockShared(&allocator->lock);
    
    return res;
}

void allocator_release_node(struct node_allocator *allocator, int64_t node_id, int32_t exclusive)
{
    assert(node_id != INVALID_NODE_ID);
    
    AcquireSRWLockShared(&allocator->lock);
    
    /* release node */
    struct node *res = allocator->nodes[node_id];
    assert(res != NULL);
    assert(res != UNLOADED_NODE);
    
    if (exclusive)
    {
        ReleaseSRWLockExclusive(&res->lock);
    }
    else
    {
        ReleaseSRWLockShared(&res->lock);
    }
        
    ReleaseSRWLockShared(&allocator->lock);
}
