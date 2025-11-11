#define __USE_MINGW_ANSI_STDIO 1
#include "tree.h"
#include "worker.h"
#include "stdio.h"


#if 1
    #define log(...)
#else
    #define log printf
#endif


/* must have exclusive access to pool object */
static void pool_add_to_event_list(struct worker_pool *pool, struct worker_task_id task_id, void *event)
{
    if (pool->wait_list_len >= pool->wait_list_alloc)
    {
        pool->wait_list_alloc = 2 * pool->wait_list_alloc + !pool->wait_list_alloc;
        void *new_ptr = realloc(pool->wait_list, sizeof(*pool->wait_list) * pool->wait_list_alloc);
        if (new_ptr == NULL)
        {
            fprintf(stderr, "Error: out of memory\n");
            return;
        }
        pool->wait_list = new_ptr;
    }
    
    /* add new event */
    int64_t pos = pool->wait_list_len++;
    pool->wait_list[pos].task_id = task_id;
    pool->wait_list[pos].event = event;
}


/* must have exclusive access to pool object */
static void pool_insert_to_free_tasks(struct worker_pool *pool, int64_t task_id)
{
    if (pool->free_id_len >= pool->free_id_alloc)
    {
        pool->free_id_alloc = 2 * pool->free_id_alloc + !pool->free_id_alloc;
        void *new_ptr = realloc(pool->free_id, sizeof(*pool->free_id) * pool->free_id_alloc);
        if (new_ptr == NULL)
        {
            fprintf(stderr, "Error: out of memory\n");
            return;
        }
        pool->free_id = new_ptr;
    }
    
    /* add new free item */
    int64_t pos = pool->free_id_len++;
    pool->free_id[pos] = task_id;
}


static void pool_realloc_tasks(struct worker_pool *pool, int64_t size)
{
    AcquireSRWLockExclusive(&pool->lock);
    if (size >= pool->tasks_alloc)
    {
        int64_t last_size = pool->tasks_alloc;
        pool->tasks_alloc = size;
        void *new_ptr = realloc(pool->tasks, sizeof(*pool->tasks) * pool->tasks_alloc);
        if (new_ptr == NULL)
        {
            fprintf(stderr, "Error: out of memory\n");
            return;
        }
        pool->tasks = new_ptr;
        memset(pool->tasks + last_size, 0, sizeof(*pool->tasks) * (pool->tasks_alloc - last_size));
        /* insert all new nodes to empty list */
        for (int64_t i = last_size; i < pool->tasks_alloc; ++i)
        {
            pool_insert_to_free_tasks(pool, i);
        }
    }
    ReleaseSRWLockExclusive(&pool->lock);
}


static DWORD WINAPI worker_fn(LPVOID lpParam)
{
    struct worker_instance *worker = (struct worker_instance *)lpParam;

    log("running thread ID: %lu\n", GetCurrentThreadId());

    atomic_store(&worker->pool->worker_start_lock, 1);

    /* get task */
    while (1)
    {
        Sleep(10);
        
        AcquireSRWLockExclusive(&worker->pool->lock);

        /* check do we need to remove this thread? */
        if (worker->pool->target_workers_len < worker->pool->workers_len &&
            worker->pool->workers_len - 1 == worker->id)
        {
            /* we need to delete this worker */
            worker->pool->workers_len--;
            ReleaseSRWLockExclusive(&worker->pool->lock);
            
            /* now - stop thread */
            CloseHandle(worker->thread);
            ExitThread(0);
            __builtin_unreachable();
        }
        
        /* else - continue working */
        
        if (worker->pool->wait_list_len == 0)
        {
            ReleaseSRWLockExclusive(&worker->pool->lock);
            continue;
        }


        log("wait list not empty: read next task from %lld [%lld]\n", worker->pool->wait_list_len - 1, worker->pool->wait_list_len);
        int64_t task_id = worker->pool->wait_list[worker->pool->wait_list_len - 1].task_id.id;
        int64_t checksum = worker->pool->wait_list[worker->pool->wait_list_len - 1].task_id.checksum;
        log("wait list not empty: read next task: %lld %lld\n", task_id, checksum);
        struct worker_task *task = worker->pool->tasks[task_id];
        log("wait list not empty: read next task: %lld %lld %p\n", task_id, checksum, task);
        void *event = worker->pool->wait_list[worker->pool->wait_list_len - 1].event;
        worker->pool->wait_list_len--;
        
        if (checksum != task->checksum)
        {
            ReleaseSRWLockExclusive(&worker->pool->lock);
            continue;
        }

        /* initializate worker */
        worker->current_task = task;
        worker->current_task_id = task_id;

        /* release pool */
        ReleaseSRWLockExclusive(&worker->pool->lock);
        
        /* process the task */
        int32_t res = setjmp(worker->jmpbuf);
        if (res == 0) 
        { 
            worker->pool->worker_process(worker, task, event); 
        } 
        else if (res == RET_EXIT)
        {
            log("Remove task_id = %lld\n", worker->current_task_id);
            
            AcquireSRWLockExclusive(&worker->pool->lock);

            free(worker->pool->tasks[worker->current_task_id]);
            worker->pool->tasks[worker->current_task_id] = NULL;
            pool_insert_to_free_tasks(worker->pool, worker->current_task_id);
            
            ReleaseSRWLockExclusive(&worker->pool->lock);
        }
        else if (res == RET_WAIT_EVENT)
        {
            continue;
        }
        else
        {
            fprintf(stderr, "Error: unknown result code from task: %d\n", res);
            continue;
        }
    }
}

static struct worker_instance *worker_create(struct worker_pool *pool, int64_t id)
{
    struct worker_instance *worker = calloc(1, sizeof(*worker));
    
    DWORD threadId;
    worker->id = id;
    worker->pool = pool;
    worker->current_task = NULL;
    worker->thread = CreateThread(
        NULL,
        0,
        worker_fn,
        worker,
        CREATE_SUSPENDED,
        &threadId
    );
    
    if (worker->thread == NULL)
    {
        fprintf(stderr, "Error: can't create worker\n");
        free(worker);
        return NULL;
    }

    return worker;
}

static void worker_run(struct worker_instance *worker)
{
    log("run worker = %p\n", worker);
    ResumeThread(worker->thread);
}

static void worker_pause(struct worker_instance *worker)
{
    SuspendThread(worker->thread);
}

static void worker_stop(struct worker_instance *worker)
{
    TerminateThread(worker->thread, 1);
    WaitForSingleObject(worker->thread, INFINITE);
    /* wait for termination */
}

static void worker_free(struct worker_instance *worker)
{
    DWORD result = WaitForSingleObject(worker->thread, 0);
    if (result != WAIT_OBJECT_0)
    {
        fprintf(stderr, "Error: trying to free not stopped worker\n");
        return;
    }
    
    free(worker);
}


/* must have exclusive access to pool object */
static void worker_pool_add_worker(struct worker_pool *pool)
{
    if (pool->workers_len >= pool->workers_alloc)
    {
        pool->workers_alloc = 2 * pool->workers_alloc + !pool->workers_alloc;
        void *new_ptr = realloc(pool->workers, sizeof(*pool->workers) * pool->workers_alloc);
        if (new_ptr == NULL)
        {
            fprintf(stderr, "Error: out of memory\n");
            return;
        }
        pool->workers = new_ptr;
    }
    
    /* add new worker */
    int64_t new_id = pool->workers_len++;
    pool->workers[new_id] = worker_create(pool, new_id);
    if (pool->workers[new_id] == NULL)
    {
        fprintf(stderr, "Error: cant create worker.\n");
        return;
    }

    pool->worker_start_lock = 0;
    worker_run(pool->workers[new_id]);
    while (!atomic_load(&pool->worker_start_lock)) 
    {
        Sleep(0);
    }
}


struct worker_pool *worker_pool_create(
    void (*worker_process)(struct worker_instance *worker, struct worker_task *task, void *event),
    void (*worker_send_event)(struct worker_task *task, void *event, void *send_data),
    void *send_data
)
{
    struct worker_pool *pool = calloc(1, sizeof(*pool));
    
    pool->workers_len = 0;
    pool->target_workers_len = 0;
    pool->workers_alloc = 4;
    pool->workers = calloc(1, sizeof(*pool->workers) * pool->workers_alloc);
    
    pool->tasks = NULL;
    pool->tasks_alloc = 0;
    
    pool->free_id = NULL;
    pool->free_id_len = 0;
    pool->free_id_alloc = 0;
    
    pool->wait_list_len = 0;
    pool->wait_list_alloc = 16;
    pool->wait_list = calloc(1, sizeof(*pool->wait_list) * pool->wait_list_alloc);
    
    pool->worker_process = worker_process;
    pool->worker_send_event = worker_send_event;
    pool->send_data = send_data;
    
    pool->lock = (SRWLOCK)SRWLOCK_INIT;
    
    return pool;
}

void worker_pool_free(struct worker_pool *pool)
{
    AcquireSRWLockExclusive(&pool->lock);
    
    for (int64_t i = 0; i < pool->workers_len; ++i)
    {
        worker_stop(pool->workers[i]);
        worker_free(pool->workers[i]);
    }
    
    free(pool->workers);
    free(pool->tasks);
    free(pool->free_id);
    free(pool->wait_list);
    free(pool);
}

/* must have exclusive access to pool object */
static int64_t pool_get_new_task_id(struct worker_pool *pool)
{
    return pool->free_id[--pool->free_id_len];
}

void worker_pool_resize_workers(struct worker_pool *pool, int64_t new_workers_count)
{
    log("Creating: pool=%p\n", pool);
    AcquireSRWLockExclusive(&pool->lock);
    
    pool->target_workers_len = new_workers_count;

    if (pool->workers_len >= pool->target_workers_len)
    {
        ReleaseSRWLockExclusive(&pool->lock);
        return;
    }
    
    /* add N new workers */
    while (pool->workers_len < pool->target_workers_len)
    {
        worker_pool_add_worker(pool);   
    }
    
    ReleaseSRWLockExclusive(&pool->lock);
    log("All threads created. Release lock\n");
    
    /* if need to remove some workers, it happens than some worker with last ID ends it's job */
}


struct worker_task_id worker_pool_start_task(struct worker_pool *pool, void *data)
{
    AcquireSRWLockShared(&pool->lock);
    if (pool->free_id_len == 0)
    {
        /* need to realloc tasks */
        ReleaseSRWLockShared(&pool->lock);
        
        pool_realloc_tasks(pool, pool->tasks_alloc * 2 + !pool->tasks_alloc);
    }   
    else
    {
        ReleaseSRWLockShared(&pool->lock);
    }
    
    /* now - insert new task */
    AcquireSRWLockExclusive(&pool->lock);   
    
    int64_t task_id = pool_get_new_task_id(pool);
    int64_t checksum = (rand() ^ (rand() << 16));

    if (pool->tasks[task_id] == NULL)
    {
        pool->tasks[task_id] = calloc(1, sizeof(*pool->tasks[task_id]));
    }
    pool->tasks[task_id]->id = task_id;
    pool->tasks[task_id]->checksum = checksum;
    pool->tasks[task_id]->data = data;
    pool->tasks[task_id]->event = NULL;
       
    ReleaseSRWLockExclusive(&pool->lock);

    /* add initialization event to this task */

    worker_pool_receive_event(pool, (struct worker_task_id){task_id, checksum}, NULL);
    
    return (struct worker_task_id){task_id, checksum};
}


void worker_pool_receive_event(struct worker_pool *pool, struct worker_task_id task_id, void *event)
{
    AcquireSRWLockExclusive(&pool->lock);   
    
    /* check - does event exists */
    if (task_id.id >= pool->tasks_alloc || 
        pool->tasks[task_id.id] == NULL || 
        pool->tasks[task_id.id]->checksum != task_id.checksum)
    {
        ReleaseSRWLockExclusive(&pool->lock);
        return;
    }
    
    /* add event */
    pool_add_to_event_list(pool, task_id, event);
    
    ReleaseSRWLockExclusive(&pool->lock);
}

/* internal api [to call from worker_callback] */

void worker_pool_exit(struct worker_instance *worker)
{
    log("Exit...\n");
    longjmp(worker->jmpbuf, RET_EXIT);
    __builtin_unreachable();
}

void worker_pool_wait_event(struct worker_instance *worker)
{
    log("Wait event...\n");
    longjmp(worker->jmpbuf, RET_WAIT_EVENT);
    __builtin_unreachable();
}

void worker_pool_send_event(struct worker_instance *worker, void *event)
{
    log("Send event %p...\n", event);
    worker->pool->worker_send_event(worker->current_task, event, worker->pool->send_data);
}
