#define __USE_MINGW_ANSI_STDIO 1
#include "stdlib.h"
#include "assert.h"
#include "stdio.h"
#include "tree.h"

struct node_allocator *allocator_create(const char *db_filename)
{
    struct node_allocator *allocator = calloc(1, sizeof(*allocator));
    
    allocator->nodes_alloc = 1024;
    allocator->nodes = calloc(1, sizeof(*allocator->nodes) * allocator->nodes_alloc);
    allocator->lock = (SRWLOCK)SRWLOCK_INIT;
    allocator->loaded_nodes = 0;
    allocator->pagefile = INVALID_HANDLE_VALUE;

    if (db_filename != NULL)
    {
        allocator->pagefile = CreateFile(
            db_filename,
            GENERIC_READ | GENERIC_WRITE,
            0,
            NULL,
            CREATE_NEW,
            FILE_ATTRIBUTE_NORMAL,
            NULL
        );
        printf("Open file!\n");
    }
    
    AcquireSRWLockExclusive(&allocator->lock);
    
    /* start optimization process */
    // TODO:
    
    ReleaseSRWLockExclusive(&allocator->lock);
    
    return allocator;
}



static struct node *allocator_load_node(struct node_allocator *allocator, int64_t node_id, int32_t exclusive)
{
    AcquireSRWLockShared(&allocator->lock);

    if (allocator->nodes[node_id] == NULL)
    {
        return NULL;
        ReleaseSRWLockShared(&allocator->lock);
    }
    if (allocator->nodes[node_id] != UNLOADED_NODE)
    {
        if (exclusive)
        {
            AcquireSRWLockExclusive(&allocator->nodes[node_id]->lock);
        }
        else
        {
            AcquireSRWLockShared(&allocator->nodes[node_id]->lock);
        }
        struct node *node = allocator->nodes[node_id];
        ReleaseSRWLockShared(&allocator->lock);
        return node;
    }
    ReleaseSRWLockShared(&allocator->lock);
    
    AcquireSRWLockExclusive(&allocator->lock);
    
    assert(allocator->pagefile != INVALID_HANDLE_VALUE);

    /* need to load node from file */
    // void *paged_memory = 
    
    fprintf(stderr, "Not implemented: loading/unloading nodes\n");
    *(int *)NULL = 1;
    // TODO:
    
    ReleaseSRWLockExclusive(&allocator->lock);
}


/* at this call, allocator must have shared access locked */
static void allocator_sync_node(struct node_allocator *allocator, int64_t node_id)
{
    printf("Sync node %lld\n", node_id);

    struct node *node = allocator->nodes[node_id];

    if (node == NULL)
    {
        printf("It is free\n");
        return;
    }
    else if (node == UNLOADED_NODE)
    {
        printf("It is already unloaded\n");
        return;
    }

    /* now we need to store node to file */
    assert(allocator->pagefile != INVALID_HANDLE_VALUE);

    int64_t offset = node_id * MAX_NODE_SIZE;
    int64_t node_size = node_get_size(node);

    printf("Writing to file at %lld count %lld\n", offset, node_size);

    
    
    fprintf(stderr, "Not implemented: loading/unloading nodes\n");
    *(int *)NULL = 1;
    // TODO:
}


static void allocator_unload_node(struct node_allocator *allocator, int64_t node_id)
{
    AcquireSRWLockShared(&allocator->lock);
    assert(allocator->pagefile != INVALID_HANDLE_VALUE);

    allocator_sync_node(allocator, node_id);
    fprintf(stderr, "Not implemented: loading/unloading nodes\n");
    *(int *)NULL = 1;
    // TODO:
    
    ReleaseSRWLockShared(&allocator->lock);
}


/* at this call, allocator must have exclusive access locked */
static void allocator_free_memory(struct node_allocator *allocator)
{
    for (int i = 0; i < allocator->nodes_len; ++i)
    {
        if (allocator->nodes[i] == UNLOADED_NODE)
        {
            assert(allocator->pagefile != INVALID_HANDLE_VALUE);
            
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
}


void allocator_sync_and_free(struct node_allocator *allocator)
{
    /* one global sync, to assert that all nodes was synced, (for example
       there was no new allocations in parallel with this sync) */
    AcquireSRWLockExclusive(&allocator->lock);
    
    for (int i = 0; i < allocator->nodes_alloc; ++i)
    {
        allocator_sync_node(allocator, i);
    }
    
    allocator_free_memory(allocator);
}


void allocator_free(struct node_allocator *allocator)
{
    /* lock */
    AcquireSRWLockExclusive(&allocator->lock);
    /* stop all other processes? */
    // TODO:
    
    
    /* free allocator */
    allocator_free_memory(allocator);
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
        res = allocator_load_node(allocator, node_id, exclusive);
        
        AcquireSRWLockShared(&allocator->lock);
        
        
        /* check - is node loaded or freed? 
           (if node was freed before loading) */
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

void allocator_release_node_by_id(struct node_allocator *allocator, int64_t node_id, int32_t exclusive)
{
    assert(node_id != INVALID_NODE_ID);
    
    AcquireSRWLockShared(&allocator->lock);
    
    struct node *node = allocator->nodes[node_id];
    allocator_release_node(allocator, node, exclusive);
        
    ReleaseSRWLockShared(&allocator->lock);
}

void allocator_release_node(struct node_allocator *allocator, struct node *node, int32_t exclusive)
{
    (void)allocator;
    
    /* release node */
    assert(node != NULL);
    assert(node != UNLOADED_NODE);
    
    if (exclusive)
    {
        ReleaseSRWLockExclusive(&node->lock);
    }
    else
    {
        ReleaseSRWLockShared(&node->lock);
    }
}
