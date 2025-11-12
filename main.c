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
    // if (setlocale(LC_ALL, "") == NULL) {
    //     perror("setlocale failed");
    //     return 1;
    // }
    
    struct tree *x = tree_create();
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
        struct tree_iterator *it = tree_iterator_create(x, v);
        struct tree_set_leaf_result res = tree_set_leaf(x, it, 1, &r1);
        v = res.version_id;
        tree_iterator_free(it);
    }
    {
        struct tree_iterator *it = tree_iterator_create(x, v);
        struct tree_split_node_result res = tree_split_node(x, it, 1, &q1);
        v = res.version_id;
        tree_iterator_free(it);
    }
    {
        struct tree_iterator *it = tree_iterator_create(x, v);
        struct tree_set_leaf_result res = tree_set_leaf(x, it, 0, &r2);
        v = res.version_id;
        tree_iterator_free(it);
    }
    {
        struct tree_iterator *it = tree_iterator_create(x, v);
        struct tree_split_node_result res = tree_split_node(x, it, 1, &q2);
        v = res.version_id;
        tree_iterator_free(it);
    }
    {
        struct tree_iterator *it = tree_iterator_create(x, v);
        tree_iterator_try_move_down(it, 1);
        struct tree_split_node_result res = tree_split_node(x, it, 0, &q3);
        v = res.version_id;
        tree_iterator_free(it);
    }
    {
        struct node_allocator *a = tree_get_allocator(x);
        allocator_try_unload_node(a, 5);
    }
    {
        struct tree_iterator *it = tree_iterator_create(x, v);
        tree_iterator_try_move_down(it, 1);
        struct tree_set_leaf_result res = tree_set_leaf(x, it, 1, &r3);
        v = res.version_id;
        tree_iterator_free(it);
    }

    /* dump tree */
    
    /* check workers */
    printf("Checking workers subsystem");
    {
        struct worker_pool *p = worker_pool_create(
            akinator_worker,
            akinator_worker_send,
            NULL
        );

        worker_pool_resize_workers(p, 4);

        struct akinator_user user = {.id = 5};
        
        while (1)
        {
            struct akinator_task_data task_data;

            task_data.dont_solved = 0;
            task_data.user = &user;
            task_data.iterator = tree_iterator_create(x, x->versions_len - 1);
            task_data.add_new_name = NULL;
            
            printf("Starting game...\n");
            struct worker_task_id task = worker_pool_start_task(p, &task_data);
    
            while (p->free_id_len != p->tasks_alloc)
            {
                char *text = malloc(1000);
                printf("ANSWER: ");
                {
                    int c, id = 0;
                    while ((c = getchar()) != '\n')
                    {
                        text[id++] = c;
                    }
                    text[id++] = 0;
                }
                if (p->free_id_len != p->tasks_alloc)
                {
                    worker_pool_receive_event(p, task, (void *)text);
                }
            }

            if (task_data.add_new_name)
            {
                free(task_data.add_new_name);
            }

            tree_iterator_free(task_data.iterator);
    
            printf("Game ended\n");
        }

        printf("You selected to quit the game\n");

        worker_pool_free(p);
    }

    printf("saving...\n");
    struct timeval tv;
    gettimeofday(&tv, NULL);
    double total_seconds1 = (double)tv.tv_sec + (double)tv.tv_usec / 1000000.0;

    tree_sync_and_free(x);

    gettimeofday(&tv, NULL);
    double total_seconds2 = (double)tv.tv_sec + (double)tv.tv_usec / 1000000.0;
    printf("saving used %f seconds\n", total_seconds2 - total_seconds1);
    
    return 0;
}

