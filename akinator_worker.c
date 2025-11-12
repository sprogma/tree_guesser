#define __USE_MINGW_ANSI_STDIO 1
#include "tree.h"
#include "worker.h"
#include "assert.h"
#include "akinator_types.h"
#include "stdio.h"
#include "stdlib.h"


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
        worker_pool_wait_event(wk);
    }
    else
    {
        struct node_variant *variant = (struct node_variant *)node;

        char *str = malloc(1000);
        sprintf(str, "my next question: %s", variant->question->text);
        worker_pool_send_event(wk, str);
        worker_pool_wait_event(wk);
    }
}


void akinator_worker(struct worker_instance *wk, struct worker_task *tsk, void *event)
{
    printf("Invoke task %p\n", tsk->data);
    struct akinator_task_data *data = tsk->data;
    
    if (event == NULL)
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
        sprintf(str, "OK. I add this to my database [not really, ahahahahaha!] [string %s]", (char *)event);
        worker_pool_send_event(wk, str);
        
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
    
    struct node_allocator *allocator = tree_get_allocator(data->iterator->tree);
    struct node *node = allocator_acquire_node(allocator, node_id, 0);

    
    if (node->type == NODE_LEAF)
    {
        struct node_leaf *leaf = (struct node_leaf *)node;

        (void)leaf;

        if (event_value == 1)
        {
            fprintf(stderr, "I was right. Don't cry )\n");
            worker_pool_exit(wk);
        }
        else
        {
            data->dont_solved = 1;
            // while (tree_iterator_get_node(data->iterator, 0) != INVALID_NODE_ID)
            // {
            //     tree_iterator_move_up(data->iterator);
            // }
            akinator_ask_question(wk, tsk);
        }   
    }
    else
    {
        struct node_variant *variant = (struct node_variant *)node;
        (void)variant;

        if (event_value == 1)
        {
            tree_iterator_move_down(data->iterator, 0);
        }
        else if (event_value == 0)
        {
            tree_iterator_move_down(data->iterator, 1);
        }
    }

    
    akinator_ask_question(wk, tsk); 
}


void akinator_worker_send(struct worker_task *tsk, void *event, void *send_data)
{
    (void)tsk;
    (void)send_data;
    printf("AKINATOR: >>> %s\n", (char *)event);
    free(event);
}
