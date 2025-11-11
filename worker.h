#ifndef WORKER
#define WORKER

#include "tree.h"
#include "windows.h"

struct worker_instance
{
    HANDLE thread;
};

struct worker_task
{
    int64_t id;
    void *data;
    void *event;
};

struct worker_pool
{
    struct worker_instance **workers;
    int64_t workers_len;
    int64_t workers_alloc;

    int64_t target_workers_len;
    
    struct worker_task *tasks;
    int64_t tasks_len;
    int64_t tasks_alloc;

    int64_t *wait_list;
    int64_t wait_list_len;
    int64_t wait_list_alloc;

    void (*worker_process)(struct worker_task *task);
    void (*worker_send_event)(struct worker_task *task, void *event);
    
    SRWLOCK lock;
};


struct worker_pool *worker_pool_create(
    void (*worker_process)(struct worker_task *task), 
    void (*worker_send_event)(struct worker_task *task, void *event)
);

void worker_pool_free(struct worker_pool *pool);

void worker_pool_resize_workers(struct worker_pool *pool, int64_t new_workers_count);

int64_t worker_pool_start_task(struct worker_pool *pool, void *data);

void worker_pool_receive_event(struct worker_pool *pool, int64_t task_id, void *event);

/* internal api [to call from worker_callback] */
void worker_pool_wait_event(struct worker_pool *pool);
void worker_pool_send_event(struct worker_pool *pool, void *event);

#endif
