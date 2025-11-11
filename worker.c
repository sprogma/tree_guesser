#include "tree.h"
#include "worker.h"
#include "stdio.h"



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
