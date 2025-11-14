#define __USE_MINGW_ANSI_STDIO 1
#include "tree.h"
#include "worker.h"
#include "assert.h"
#include "akinator_types.h"
#include "stdio.h"
#include "stdlib.h"


#define lock_log(...)


void akinator_ask_question(struct worker_instance *wk, struct worker_task *tsk)
{
    struct akinator_task_data *data = tsk->data;
    
    /* get node from iterator */
    int64_t node_id = tree_iterator_get_node(data->iterator, 0);

    if (node_id == INVALID_NODE_ID || data->dont_solved)
    {
        char *str = malloc(1000);
        sprintf(str, "I don't know this creature :(");
        worker_pool_send_event(wk, str);
        
        str = malloc(1000);
        sprintf(str, "Enter his name:");
        worker_pool_send_event(wk, str);

        data->dont_solved = 1;
        worker_pool_wait_event(wk);
    }
    
    struct node_allocator *allocator = tree_get_allocator(data->iterator->tree);
    struct node *node = allocator_acquire_node(allocator, node_id, 0);

    if (node->type == NODE_LEAF)
    {
        struct node_leaf *leaf = (struct node_leaf *)node;
        
        char *str = malloc(1000);
        sprintf(str, "I think i know that is it! it is %s?", leaf->record->name);
        worker_pool_send_event(wk, str);
        allocator_release_node(allocator, node, 0);
        worker_pool_wait_event(wk);
    }
    else
    {
        struct node_variant *variant = (struct node_variant *)node;

        char *str = malloc(1000);
        sprintf(str, "my next question: %s", variant->question->text);
        worker_pool_send_event(wk, str);
        allocator_release_node(allocator, node, 0);
        worker_pool_wait_event(wk);
    }
}


void akinator_worker(struct worker_instance *wk, struct worker_task *tsk, void *event)
{
    struct akinator_task_data *data = tsk->data;

    if (data->add_new_name != NULL)
    {
        char *str = malloc(1000);
        sprintf(str, "Yes, now i know it.");
        worker_pool_send_event(wk, str);

        struct question *question = malloc(sizeof(*question));
        question->text = strdup(event);
        struct tree_split_node_result res = tree_split_node(data->iterator->tree, data->iterator, 1, question);

        /* and add new record */

        struct tree_iterator *it = tree_iterator_create(data->iterator->tree, res.version_id);
        for (int i = 0; i + 1 < data->iterator->path_len; ++i)
        {
            int64_t n_id = tree_iterator_get_node(data->iterator, data->iterator->path_len - i - 2);
            int64_t x_id = tree_iterator_get_node(data->iterator, data->iterator->path_len - i - 1);
            struct node_allocator *allocator = tree_get_allocator(data->iterator->tree);
            struct node_variant *x = (struct node_variant *)allocator_acquire_node(allocator, x_id, 0);
            assert(x->type == NODE_VARIANT);
            if (n_id == x->l)
            {
                tree_iterator_try_move_down(it, 1);
            }
            else
            {
                tree_iterator_try_move_down(it, 0);
            }
            allocator_release_node(allocator, (struct node *)x, 0);
        }

        assert(tree_iterator_get_node(it, 0) == res.new_node_id);
        
        struct record *record = malloc(sizeof(*record));
        record->name = data->add_new_name;
        tree_set_leaf(it->tree, it, 0, record);
        
        data->add_new_name = NULL;

        tree_iterator_free(it);
        
        free(event);
        worker_pool_exit(wk);
    }
    else if (event == NULL)
    {
        char *str = malloc(1000);
        sprintf(str, "Hello user %lld! Let me ask my questions...", data->user->id);
        worker_pool_send_event(wk, str);
        
        akinator_ask_question(wk, tsk);
    }
    else if (data->dont_solved)
    {
        /* update tree */
        char *str = malloc(1000);
        sprintf(str, "OK. I add this to my database [string %s]", (char *)event);
        worker_pool_send_event(wk, str);
        
        /* add new node */
        {
            int64_t node_id = tree_iterator_get_node(data->iterator, 0);
        
            if (node_id == INVALID_NODE_ID)
            {
                /* this is empty tree - add new character */
                struct record *record = malloc(sizeof(*record));
                record->name = strdup(event);
                tree_set_leaf(data->iterator->tree, data->iterator, 0, record);
            }
            else
            {
                struct node_allocator *allocator = tree_get_allocator(data->iterator->tree);
                struct node *node = allocator_acquire_node(allocator, node_id, 0);
                
                if (node->type == NODE_LEAF)
                {
                    /* need to split node */
                    /* ask for new question */
                    data->add_new_name = strdup(event);
                    
                    char *str = malloc(1000);
                    sprintf(str, "Ok. Your %s is not %s becouse %s ..?\n", data->add_new_name, ((struct node_leaf *)node)->record->name, data->add_new_name);
                    worker_pool_send_event(wk, str);
                    
                    allocator_release_node(allocator, node, 0);
                    worker_pool_wait_event(wk);
                }
                else
                {
                    allocator_release_node(allocator, node, 0);
                    struct record *record = malloc(sizeof(*record));
                    record->name = strdup(event);
                    tree_set_leaf(data->iterator->tree, data->iterator, data->set_as_leaf, record);
                }
            }
        }
        
        free(event);
        worker_pool_exit(wk);
    }

    int64_t event_value = atoi((char *)event);
    free(event);

    /* else - move iterator */
    int64_t node_id = tree_iterator_get_node(data->iterator, 0);

    if (node_id == INVALID_NODE_ID)
    {
        fprintf(stderr, "Internal akinator_worker error\n");
        worker_pool_exit(wk);
    }

    lock_log("UPDATE USING ANSWER\n");
    struct node_allocator *allocator = tree_get_allocator(data->iterator->tree);
    struct node *node = allocator_acquire_node(allocator, node_id, 0);

    
    if (node->type == NODE_LEAF)
    {
        struct node_leaf *leaf = (struct node_leaf *)node;

        (void)leaf;

        if (event_value == 1)
        {
            char *str = malloc(1000);
            sprintf(str, "I was right. Don't cry )\n");
            worker_pool_send_event(wk, str);
            allocator_release_node(allocator, node, 0);
            worker_pool_exit(wk);
        }
        else
        {
            data->dont_solved = 1;
            // while (tree_iterator_get_node(data->iterator, 0) != INVALID_NODE_ID)
            // {
            //     tree_iterator_move_up(data->iterator);
            // }
            allocator_release_node(allocator, node, 0);
            akinator_ask_question(wk, tsk);
        }   
    }
    else
    {
        struct node_variant *variant = (struct node_variant *)node;
        (void)variant;

        if (event_value == 1)
        {
            if (tree_iterator_try_move_down(data->iterator, 0) != 0)
            {
                data->set_as_leaf = 0;
                data->dont_solved = 1;
            }
        }
        else if (event_value == 0)
        {
            if (tree_iterator_try_move_down(data->iterator, 1) != 0)
            {
                data->set_as_leaf = 1;
                data->dont_solved = 1;
            }
        }
    }

    
    allocator_release_node(allocator, node, 0);
    akinator_ask_question(wk, tsk); 
}


void akinator_worker_send(struct worker_task *tsk, void *event, void *send_data)
{
    (void)tsk;
    (void)send_data;
    printf("AKINATOR: >>> %s\n", (char *)event);
    free(event);
}

void *akinator_worker_prompt()
{
    // lock_log("ANSWER: ");
    char *text = malloc(1000);
    {
        int c, id = 0;
        while ((c = getchar()) != '\n')
        {
            text[id++] = c;
        }
        text[id++] = 0;
    }
    return text;
}

struct akinator_user *akinator_worker_connect_user(struct tree *tree)
{
    (void)tree;
    
    struct akinator_user *user = malloc(sizeof(*user));

    user->id = rand();
    printf("User %lld connected\n", user->id);
    
    return user;
}

struct akinator_task_data *akinator_worker_start_game(struct akinator_user *user, struct tree *tree)
{
    struct akinator_task_data *task_data = malloc(sizeof(*task_data));
    
    task_data->dont_solved = 0;
    task_data->user = user;
    task_data->iterator = tree_iterator_create(tree, tree->versions_len - 1);
    task_data->add_new_name = NULL;

    return task_data;
}

void akinator_worker_finalize_game(struct akinator_task_data *task_data, struct akinator_user *user, struct tree *tree)
{
    (void)user;
    (void)tree;
    
    printf("Game ended.\n");
    
    if (task_data->add_new_name)
    {
        free(task_data->add_new_name);
    }

    tree_iterator_free(task_data->iterator);

    free(task_data);
}

int32_t akinator_worker_restart_game(struct akinator_user *user, struct tree *tree)
{
    (void)user;
    (void)tree;
    printf("Do you want to restart? [y/n] ");
    char c;
    scanf("%c", &c);
    
    while (getchar() != '\n') {}
    
    return c == 'y' || c == 'Y';
}

void akinator_worker_disconnect_user(struct akinator_user *user, struct tree *tree)
{
    (void)tree;
    
    printf("User %lld disconnected\n", user->id);
    free(user);
}
