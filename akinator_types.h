#ifndef AKINATOR_TYPES
#define AKINATOR_TYPES

#include "tree.h"
#include "worker.h"

struct akinator_user
{
    int64_t id;
};

struct akinator_task_data
{
    int64_t dont_solved;
    struct akinator_user *user;
    struct tree_iterator *iterator;
};


void akinator_worker(struct worker_instance *wk, struct worker_task *tsk, void *event);
void akinator_worker_send(struct worker_task *tsk, void *event, void *send_data);


#endif
