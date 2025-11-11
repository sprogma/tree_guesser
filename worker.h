#ifndef WORKER
#define WORKER

#include "tree.h"
#include "windows.h"
#include "setjmp.h"
#include "stdatomic.h"

struct worker_instance
{
    int64_t id;
    HANDLE thread;
    struct worker_pool *pool;
    struct worker_task *current_task;
    int64_t current_task_id;
    jmp_buf jmpbuf;
};

struct worker_task
{
    int64_t id;
    int64_t checksum;
    void *data;
    void *event;
};

struct worker_task_id
{
    int64_t id;
    int64_t checksum;
};

struct pool_invoke_event
{
    struct worker_task_id task_id;
    void *event;
};

struct worker_pool
{
    struct worker_instance **workers;
    int64_t workers_len;
    int64_t workers_alloc;

    int64_t target_workers_len;
    
    struct worker_task **tasks;
    int64_t tasks_alloc;
    
    /* to get tasks len, take 'tasks_alloc - free_id_len' */

    struct pool_invoke_event *wait_list;
    int64_t wait_list_len;
    int64_t wait_list_alloc;

    int64_t *free_id;
    int64_t free_id_len;
    int64_t free_id_alloc;

    void (*worker_process)(struct worker_instance *worker, struct worker_task *task, void *event);
    void (*worker_send_event)(struct worker_task *task, void *event, void *send_data);
    void *send_data;
    
    SRWLOCK lock;
    _Atomic int worker_start_lock;
};


struct worker_pool *worker_pool_create(
    void (*worker_process)(struct worker_instance *worker, struct worker_task *task, void *event),
    void (*worker_send_event)(struct worker_task *task, void *event, void *send_data),
    void *send_data;
);

void worker_pool_free(struct worker_pool *pool);

void worker_pool_resize_workers(struct worker_pool *pool, int64_t new_workers_count);

struct worker_task_id worker_pool_start_task(struct worker_pool *pool, void *data);

void worker_pool_receive_event(struct worker_pool *pool, struct worker_task_id task_id, void *event);

/* internal api [to call from worker_callback] */
#define RET_WAIT_EVENT 0x2
#define RET_EXIT 0x1
void worker_pool_exit(struct worker_instance *worker);
void worker_pool_wait_event(struct worker_instance *worker);
void worker_pool_send_event(struct worker_instance *worker, void *event);

#endif
