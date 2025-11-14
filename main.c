#define __USE_MINGW_ANSI_STDIO 1
#include "sys/time.h"
#include "stdio.h"
#include "tree.h"
#include "worker.h"
#include "locale.h"
#include "akinator_types.h"


int main()
{
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    
    struct tree *tree = tree_create();
    int64_t v = 0;

    struct question 
        q1 = {.text = "Z SVO GOOOL GOOOL ABOBA?"},
        q2 = {.text = "GOIDA?"},
        q3 = {.text = "LOLOLOLOLOLOLOLOLOLO?"};

    struct record
        r1 = {.name = "Mr. ZV"},
        r2 = {.name = "Bear [gooooooal]"},
        r3 = {.name = "Me"};

    /* insert node to tree */
    {
        struct tree_iterator *it = tree_iterator_create(tree, v);
        struct tree_set_leaf_result res = tree_set_leaf(tree, it, 1, &r1);
        v = res.version_id;
        tree_iterator_free(it);
    }
    {
        struct tree_iterator *it = tree_iterator_create(tree, v);
        struct tree_split_node_result res = tree_split_node(tree, it, 1, &q1);
        v = res.version_id;
        tree_iterator_free(it);
    }
    {
        struct tree_iterator *it = tree_iterator_create(tree, v);
        struct tree_set_leaf_result res = tree_set_leaf(tree, it, 0, &r2);
        v = res.version_id;
        tree_iterator_free(it);
    }
    {
        struct tree_iterator *it = tree_iterator_create(tree, v);
        struct tree_split_node_result res = tree_split_node(tree, it, 1, &q2);
        v = res.version_id;
        tree_iterator_free(it);
    }
    {
        struct tree_iterator *it = tree_iterator_create(tree, v);
        tree_iterator_try_move_down(it, 1);
        struct tree_split_node_result res = tree_split_node(tree, it, 0, &q3);
        v = res.version_id;
        tree_iterator_free(it);
    }
    {
        struct node_allocator *a = tree_get_allocator(tree);
        allocator_try_unload_node(a, 5);
    }
    {
        struct tree_iterator *it = tree_iterator_create(tree, v);
        tree_iterator_try_move_down(it, 1);
        struct tree_set_leaf_result res = tree_set_leaf(tree, it, 1, &r3);
        v = res.version_id;
        tree_iterator_free(it);
    }

    /* dump tree */
    
    /* check workers */
    {
        struct worker_pool *worker_pool = worker_pool_create(
            akinator_worker,
            akinator_worker_send,
            NULL
        );

        worker_pool_resize_workers(worker_pool, 4);

        struct akinator_user *user = akinator_worker_connect_user(tree);
        
        while (1)
        {
            struct akinator_task_data *task_data = akinator_worker_start_game(user, tree);
            
            struct worker_task_id task = worker_pool_start_task(worker_pool, task_data);
    
            while (worker_pool->free_id_len != worker_pool->tasks_alloc)
            {
                void *event = akinator_worker_prompt();
                if (worker_pool->free_id_len != worker_pool->tasks_alloc)
                {
                    worker_pool_receive_event(worker_pool, task, event);
                }
            }

            akinator_worker_finalize_game(task_data, user, tree);

            if (!akinator_worker_restart_game(user, tree))
            {
                break;
            }
        }

        worker_pool_free(worker_pool);
        
        akinator_worker_disconnect_user(user, tree);
    }

    printf("saving...\n");
    struct timeval tv;
    gettimeofday(&tv, NULL);
    double total_seconds1 = (double)tv.tv_sec + (double)tv.tv_usec / 1000000.0;

    tree_sync_and_free(tree);

    gettimeofday(&tv, NULL);
    double total_seconds2 = (double)tv.tv_sec + (double)tv.tv_usec / 1000000.0;
    printf("saving used %f seconds\n", total_seconds2 - total_seconds1);
    
    return 0;
}

