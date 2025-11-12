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
    int32_t set_as_leaf;
    char *add_new_name;
};


void akinator_worker(struct worker_instance *wk, struct worker_task *tsk, void *event);
void akinator_worker_send(struct worker_task *tsk, void *event, void *send_data);

struct akinator_user *akinator_worker_connect_user(struct tree *tree);
struct akinator_task_data *akinator_worker_start_game(struct akinator_user *user, struct tree *tree);
void *akinator_worker_prompt();
void akinator_worker_finalize_game(struct akinator_task_data *task_data, struct akinator_user *user, struct tree *tree);
/* return bool - True == restart */
int32_t akinator_worker_restart_game(struct akinator_user *user, struct tree *tree);
void akinator_worker_disconnect_user(struct akinator_user *user, struct tree *tree);

#endif
