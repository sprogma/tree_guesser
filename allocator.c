#define __USE_MINGW_ANSI_STDIO 1
#include "stdlib.h"
#include "assert.h"
#include "stdio.h"
#include "tree.h"

#if 1
    #define log(...)
#else
    #define log printf
#endif


/* must have at least shared access before this call */
static void allocator_store_size(struct node_allocator *allocator)
{
    log("store %lld nodes\n", allocator->nodes_alloc);
    DWORD written = 0;
    OVERLAPPED ov = {0};
    ov.Offset = (DWORD)(0LL & 0xFFFFFFFF);
    ov.OffsetHigh = (DWORD)(0LL >> 32);
    BOOL ok = WriteFile(allocator->pagefile, &allocator->nodes_alloc, 8, &written, &ov);
    if (!ok) 
    {
        /* no support of async IO for now */
        fprintf(stderr, "Error: WriteFile returned FALSE\n");
        return;
    }
    if (written != 8)
    {
        fprintf(stderr, "Error: WriteFile wrote not all bytes\n");
        return;
    }    
}


struct node_allocator *allocator_create(const char *db_filename)
{
    struct node_allocator *allocator = calloc(1, sizeof(*allocator));
    
    allocator->lock = (SRWLOCK)SRWLOCK_INIT;

    if (db_filename != NULL)
    {
        log("file %s\n", db_filename);
        allocator->pagefile = CreateFile(
            db_filename,
            GENERIC_READ | GENERIC_WRITE,
            0,
            NULL,
            OPEN_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            // FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH,
            NULL
        );
        log("Open file! %ld\n", GetLastError());
        assert(allocator->pagefile != INVALID_HANDLE_VALUE);

        /* try to read node count from file */
        allocator->nodes_alloc = 0;
        
        DWORD read = 0;
        OVERLAPPED ov = {0};
        ov.Offset = (DWORD)(0LL & 0xFFFFFFFF);
        ov.OffsetHigh = (DWORD)(0LL >> 32);
        BOOL ok = ReadFile(allocator->pagefile, &allocator->nodes_alloc, 8, &read, &ov);
        if (!ok) 
        {
            fprintf(stderr, "Error: ReadFile returned FALSE\n");
            return NULL;
        }
        if (read != 8)
        {
            fprintf(stderr, "Error: ReadFile read not all bytes\n");
            return NULL;
        }

        
        allocator->nodes = calloc(1, sizeof(*allocator->nodes) * allocator->nodes_alloc);
        allocator->loaded_nodes = 0;

        if (allocator->nodes_alloc == 0)
        {
            fprintf(stderr, "Error: loading database from 0 nodes\n");
            return NULL;
        }

        /* set nodes as unloaded */
        for (int i = 0; i < allocator->nodes_alloc; ++i)
        {
            /* check is it free */
            {
                int node_size = 0;
                int64_t offset = 8 + i * (MAX_NODE_SIZE + 4);
                DWORD read = 0;
                OVERLAPPED ov = {0};
                ov.Offset = (DWORD)(offset & 0xFFFFFFFF);
                ov.OffsetHigh = (DWORD)(offset >> 32);
                BOOL ok = ReadFile(allocator->pagefile, &node_size, 4, &read, &ov);
                if (!ok) 
                {
                    fprintf(stderr, "Error: ReadFile returned FALSE\n");
                    return NULL;
                }
                if (read != 4)
                {
                    fprintf(stderr, "Error: ReadFile read not all bytes\n");
                    return NULL;
                }

                if (node_size == 0)
                {
                    allocator->nodes[i] = NULL;
                }
                else
                {
                    allocator->nodes[i] = UNLOADED_NODE;
                }
            }
        }
    }
    else
    {
        allocator->nodes_alloc = 1024;
        allocator->nodes = calloc(1, sizeof(*allocator->nodes) * allocator->nodes_alloc);
        allocator->loaded_nodes = 0;
        allocator->pagefile = INVALID_HANDLE_VALUE;
    }

    allocator_store_size(allocator);
    
    AcquireSRWLockExclusive(&allocator->lock);
    
    /* start optimization process */
    // TODO:
    
    ReleaseSRWLockExclusive(&allocator->lock);
    
    return allocator;
}


void allocator_enable_pagefile(struct node_allocator *allocator, const char *db_filename)
{
    AcquireSRWLockExclusive(&allocator->lock);
    if (allocator->pagefile == INVALID_HANDLE_VALUE)
    {
        log("file %s\n", db_filename);
        allocator->pagefile = CreateFile(
            db_filename,
            GENERIC_READ | GENERIC_WRITE,
            0,
            NULL,
            CREATE_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            // FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH,
            NULL
        );
        log("Open file! %ld\n", GetLastError());
        assert(allocator->pagefile != INVALID_HANDLE_VALUE);
    }
    ReleaseSRWLockExclusive(&allocator->lock);
}


static struct node *allocator_load_node(struct node_allocator *allocator, int64_t node_id, int32_t exclusive)
{
    log("Load node %lld\n", node_id);

    // Uncomment for some kind of optimization
    
    AcquireSRWLockShared(&allocator->lock);
    struct node *node = allocator->nodes[node_id];
    if (node == NULL)
    {
        log("It is free\n");
        ReleaseSRWLockShared(&allocator->lock);
        return NULL;
    }
    else if (node != UNLOADED_NODE)
    {
        log("It is loaded\n");
        ReleaseSRWLockShared(&allocator->lock);
        return node;
    }

    assert(allocator->pagefile != INVALID_HANDLE_VALUE);
    
    HANDLE hFile = allocator->pagefile;
    
    ReleaseSRWLockShared(&allocator->lock);
    

    /* now we need to store node to file */

    int64_t offset = 8 + node_id * (MAX_NODE_SIZE + 4);
    int32_t node_size = -1;
    
    log("Reading from file at %lld count 4\n", offset);

    {
        DWORD read = 0;
        OVERLAPPED ov = {0};
        ov.Offset = (DWORD)(offset & 0xFFFFFFFF);
        ov.OffsetHigh = (DWORD)(offset >> 32);
        BOOL ok = ReadFile(hFile, &node_size, 4, &read, &ov);
        if (!ok) 
        {
            /* no support of async IO for now */
            fprintf(stderr, "Error: ReadFile returned FALSE\n");
            return NULL;
        }
        if (read != 4)
        {
            fprintf(stderr, "Error: ReadFile read not all bytes\n");
            return NULL;
        }
    }

    log("Node size is %d\n", node_size);

    if (node_size == 0)
    {
        fprintf(stderr, "Error: try to load NULL [free] node\n");
        return NULL;
    }
    if (node_size < 0 || node_size > MAX_NODE_SIZE)
    {
        fprintf(stderr, "Error: corrupted node_size\n");
        return NULL;
    }

    node = malloc(node_size);

    log("Node allocated to position %p\n", node);
    

    /* take node */

    log("Read node size: %d\n", node_size);

    node = malloc(node_size);

    {
        offset += 4;
        DWORD read = 0;
        OVERLAPPED ov = {0};
        ov.Offset = (DWORD)(offset & 0xFFFFFFFF);
        ov.OffsetHigh = (DWORD)(offset >> 32);
        BOOL ok = ReadFile(hFile, node, node_size, &read, &ov);
        if (!ok) 
        {
            /* no support of async IO for now */
            fprintf(stderr, "Error: ReadFile returned FALSE\n");
            return NULL;
        }
        if (read != (DWORD)node_size)
        {
            fprintf(stderr, "Error: ReadFile read not all bytes\n");
            return NULL;
        }
    }

    log("Node read\n");

    node->lock = (SRWLOCK)SRWLOCK_INIT;

    log("Node's lock updated\n");

    
    AcquireSRWLockExclusive(&allocator->lock);
    
    struct node *curr_node = allocator->nodes[node_id];
    
    if (curr_node == NULL)
    {
        log("CURR node is free\n");
        ReleaseSRWLockExclusive(&allocator->lock);
        free(node);
        log("ret\n");
        return NULL;
    }
    else if (curr_node != UNLOADED_NODE)
    {
        log("CURR node is loaded\n");
        free(node);
        node = allocator->nodes[node_id];
    }
    
    /* update allocator's node */
    allocator->nodes[node_id] = node;
    log("Node updated\n");

    /* get needed access mode */
    if (exclusive)
    {
        AcquireSRWLockExclusive(&node->lock);
    }
    else
    {
        AcquireSRWLockShared(&node->lock);
    }

    ReleaseSRWLockExclusive(&allocator->lock);
    
    log("Node loaded\n");
    
    return node;
}


static void allocator_sync_node(struct node_allocator *allocator, int64_t node_id, int32_t is_allocator_already_shared)
{
    log("Sync node %lld\n", node_id);
    
    int64_t offset = 8 + node_id * (MAX_NODE_SIZE + 4);

    if (!is_allocator_already_shared)
    {
        AcquireSRWLockShared(&allocator->lock);
    }
    
    struct node *node = allocator->nodes[node_id];

    if (node == NULL)
    {
        log("It is free\n");

        /* write to file ZERO size to show that node is empty */
        {
            int32_t zero = 0;
            DWORD written = 0;
            OVERLAPPED ov = {0};
            ov.Offset = (DWORD)(offset & 0xFFFFFFFF);
            ov.OffsetHigh = (DWORD)(offset >> 32);
            BOOL ok = WriteFile(allocator->pagefile, &zero, 4, &written, &ov);
            if (!ok) 
            {
                /* no support of async IO for now */
                fprintf(stderr, "Error: WriteFile returned FALSE\n");
                if (!is_allocator_already_shared)
                {
                    ReleaseSRWLockShared(&allocator->lock);
                }
                return;
            }
            if (written != 4)
            {
                fprintf(stderr, "Error: WriteFile wrote not all bytes\n");
                if (!is_allocator_already_shared)
                {
                    ReleaseSRWLockShared(&allocator->lock);
                }
                return;
            }
        }
        
        if (!is_allocator_already_shared)
        {
            ReleaseSRWLockShared(&allocator->lock);
        }
        
        return;
    }
    else if (node == UNLOADED_NODE)
    {
        log("It is already unloaded\n");
        if (!is_allocator_already_shared)
        {
            ReleaseSRWLockShared(&allocator->lock);
        }
        return;
    }

    AcquireSRWLockExclusive(&node->lock);
    
    /* now we need to store node to file */
    assert(allocator->pagefile != INVALID_HANDLE_VALUE);

    HANDLE hFile = allocator->pagefile;
    
    if (!is_allocator_already_shared)
    {
        ReleaseSRWLockShared(&allocator->lock);
    }

    /* take node */

    int32_t node_size = node_get_size(node);

    log("Writing to file at %lld count 4+%d\n", offset, node_size);

    {
        DWORD written = 0;
        OVERLAPPED ov = {0};
        ov.Offset = (DWORD)(offset & 0xFFFFFFFF);
        ov.OffsetHigh = (DWORD)(offset >> 32);
        BOOL ok = WriteFile(hFile, &node_size, 4, &written, &ov);
        if (!ok) 
        {
            /* no support of async IO for now */
            fprintf(stderr, "Error: WriteFile returned FALSE\n");
            ReleaseSRWLockExclusive(&node->lock);
            return;
        }
        if (written != 4)
        {
            fprintf(stderr, "Error: WriteFile wrote not all bytes\n");
            ReleaseSRWLockExclusive(&node->lock);
            return;
        }
    }
    
    {
        offset += 4;
        DWORD written = 0;
        OVERLAPPED ov = {0};
        ov.Offset = (DWORD)(offset & 0xFFFFFFFF);
        ov.OffsetHigh = (DWORD)(offset >> 32);
        BOOL ok = WriteFile(hFile, node, node_size, &written, &ov);
        if (!ok) 
        {
            /* no support of async IO for now */
            fprintf(stderr, "Error: WriteFile returned FALSE\n");
            ReleaseSRWLockExclusive(&node->lock);
            return;
        }
        if (written != (DWORD)node_size)
        {
            fprintf(stderr, "Error: WriteFile wrote not all bytes\n");
            ReleaseSRWLockExclusive(&node->lock);
            return;
        }
    }

    log("Node written\n");

    ReleaseSRWLockExclusive(&node->lock);
}


int32_t allocator_try_unload_node(struct node_allocator *allocator, int64_t node_id)
{
    int64_t node_copy_size = 0;
    void *node_copy = NULL;
    
    AcquireSRWLockShared(&allocator->lock);
    
    
    {
        struct node *node = allocator->nodes[node_id];

        if (node == NULL)
        {
            ReleaseSRWLockShared(&allocator->lock);
            return NODE_IS_FREE;
        }
        if (node == UNLOADED_NODE)
        {
            ReleaseSRWLockShared(&allocator->lock);
            return NODE_ALREADY_UNLOADED;
        }
        
        node_copy_size = node_get_size(node);
        node_copy = malloc(node_copy_size);
        memcpy(node_copy, node, node_copy_size);
    }

    ReleaseSRWLockShared(&allocator->lock);
    

    allocator_sync_node(allocator, node_id, 0);
    
    
    AcquireSRWLockExclusive(&allocator->lock);
    
    struct node *node = allocator->nodes[node_id];

    if (memcmp(node_copy, node, node_copy_size) != 0)
    {
        ReleaseSRWLockExclusive(&allocator->lock);
        return NODE_WAS_MODITIFIED;
    }
    
    if (node == NULL)
    {
        ReleaseSRWLockExclusive(&allocator->lock);
        return NODE_WAS_FREED;
    }
    if (node == UNLOADED_NODE)
    {
        ReleaseSRWLockExclusive(&allocator->lock);
        return NODE_ALREADY_UNLOADED;
    }
    
    /* simply unload node */
    allocator->nodes[node_id] = UNLOADED_NODE;
    log("NODE %lld is now unloaded\n", node_id);
    
    ReleaseSRWLockExclusive(&allocator->lock);    
    
    return 0;
}


void allocator_unload_node_force(struct node_allocator *allocator, int64_t node_id)
{
    AcquireSRWLockExclusive(&allocator->lock);
    
    allocator_sync_node(allocator, node_id, 1);
    
    struct node *node = allocator->nodes[node_id];
   
    if (node == NULL)
    {
        ReleaseSRWLockExclusive(&allocator->lock);
        return;
    }
    if (node == UNLOADED_NODE)
    {
        ReleaseSRWLockExclusive(&allocator->lock);
        return;
    }
    
    /* simply unload node */
    allocator->nodes[node_id] = UNLOADED_NODE;
    log("NODE %lld is now unloaded\n", node_id);
    
    ReleaseSRWLockExclusive(&allocator->lock);    
}


/* at this call, allocator must have exclusive access locked */
static void allocator_free_memory(struct node_allocator *allocator)
{
    for (int i = 0; i < allocator->nodes_alloc; ++i)
    {
        if (allocator->nodes[i] == UNLOADED_NODE)
        {
            // Nothing to do
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

    allocator_store_size(allocator);

    int node_id = 0;
    for (int i = 0; i < allocator->nodes_alloc; ++i)
    {
        allocator_sync_node(allocator, node_id, 1);
        node_id = node_id + 179;
        node_id %= allocator->nodes_alloc;
    }
    
    allocator_free_memory(allocator);
}


void allocator_free(struct node_allocator *allocator)
{
    /* lock */
    AcquireSRWLockExclusive(&allocator->lock);
    /* stop all other processes? */
    // TODO:
    
    allocator_store_size(allocator);
    
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

    allocator->nodes = new_ptr;
    
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
    
    allocator_store_size(allocator);
    
    ReleaseSRWLockExclusive(&allocator->lock);
    
    return (struct allocator_create_node_result){result_node_id, result_node};
}


struct node *allocator_acquire_node(struct node_allocator *allocator, int64_t node_id, int32_t exclusive)
{
    assert(node_id != INVALID_NODE_ID);
    
    AcquireSRWLockShared(&allocator->lock);
    
    /* if node was unloaded */
    struct node *res = allocator->nodes[node_id];
    log("NODE %lld [%p] ACQUIRED\n", node_id, res);
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
        log("[after load it is %p]\n", res);
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
    log("NODE %lld RELEASED BY ID\n", node_id);
    assert(node_id != INVALID_NODE_ID);
    
    AcquireSRWLockShared(&allocator->lock);
    
    struct node *node = allocator->nodes[node_id];
    allocator_release_node(allocator, node, exclusive);
        
    ReleaseSRWLockShared(&allocator->lock);
}

void allocator_release_node(struct node_allocator *allocator, struct node *node, int32_t exclusive)
{
    log("NODE %p RELEASED\n", node);
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
