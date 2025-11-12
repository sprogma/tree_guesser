#define __USE_MINGW_ANSI_STDIO 1
#include "sys/time.h"
#include "stdio.h"
#include "tree.h"
#include "worker.h"
#include "akinator_types.h"

#define PRINT_INDENT(indent) for (int i = 0; i < 4 * (indent); ++i) { putchar(' '); }
void print_tree(struct node_allocator *a, int64_t node_id, int indent)
{
    if (node_id == INVALID_NODE_ID)
    {
        PRINT_INDENT(indent);
        printf("no node\n");
    }
    else
    {
        struct node *node = allocator_acquire_node(a, node_id, 0);
        if (node->type == NODE_VARIANT)
        {
            int64_t l_id, r_id;
            l_id = ((struct node_variant *)node)->l;
            r_id = ((struct node_variant *)node)->r;
            allocator_release_node(a, node, 0);
            print_tree(a, l_id, indent + 1);
            PRINT_INDENT(indent);
            printf("node VARIANT %lld [%p]: type: %d ptr_count: %d data:%p\n", node_id, node, node->type, node->ptr_count, ((struct node_variant *)node)->question);
            print_tree(a, r_id, indent + 1);
        }
        else
        {
            PRINT_INDENT(indent);
            printf("node LEAF %lld [%p]: type: %d ptr_count: %d data:%p\n", node_id, node, node->type, node->ptr_count, ((struct node_leaf *)node)->record);
            allocator_release_node(a, node, 0);
        }
    }
}


void dump_version(struct node_allocator *a, struct tree_version *v)
{
    printf("-------- version %p: parent: %lld [far: %lld] depth: %lld\n", v, v->parent, v->far_parent, v->depth);
    print_tree(a, v->root, 0);
}


void dump_tree(struct tree *t, int64_t version_id)
{
    printf("----- tree %p: total %lld versions\n", t, t->versions_len);
    printf("-------- tree of version %lld:\n", version_id);

    struct tree_version *version = tree_acquire_version(t, version_id, 0);
    struct node_allocator *a = tree_get_allocator(t);
    dump_version(a, version);
    tree_release_version(t, version, 0);
}


int main()
{
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
        printf("1 created: %lld %lld\n", res.version_id, res.new_node_id);
        v = res.version_id;
    }
    dump_tree(x, v);
    {
        struct tree_iterator *it = tree_iterator_create(x, v);
        struct tree_split_node_result res = tree_split_node(x, it, 1, &q1);
        printf("2 splittd: %lld %lld\n", res.version_id, res.new_node_id);
        v = res.version_id;
    }
    dump_tree(x, v);
    {
        struct tree_iterator *it = tree_iterator_create(x, v);
        struct tree_set_leaf_result res = tree_set_leaf(x, it, 0, &r2);
        printf("3 created: %lld %lld\n", res.version_id, res.new_node_id);
        v = res.version_id;
    }
    dump_tree(x, v);
    {
        struct tree_iterator *it = tree_iterator_create(x, v);
        struct tree_split_node_result res = tree_split_node(x, it, 1, &q2);
        printf("4 splittd: %lld %lld\n", res.version_id, res.new_node_id);
        v = res.version_id;
    }
    dump_tree(x, v);
    {
        struct tree_iterator *it = tree_iterator_create(x, v);
        tree_iterator_move_down(it, 1);
        struct tree_split_node_result res = tree_split_node(x, it, 0, &q3);
        printf("5 splittd: %lld %lld\n", res.version_id, res.new_node_id);
        v = res.version_id;
    }
    dump_tree(x, v);
    printf("UNLOADING-----------------------------------5 node\n");
    {
        struct node_allocator *a = tree_get_allocator(x);
        allocator_try_unload_node(a, 5);
    printf("UNLOADING END-----------------------------------5 node\n");
    dump_tree(x, v);
    }
    {
        struct tree_iterator *it = tree_iterator_create(x, v);
        tree_iterator_move_down(it, 1);
        struct tree_set_leaf_result res = tree_set_leaf(x, it, 1, &r3);
        printf("6 created: %lld %lld\n", res.version_id, res.new_node_id);
        v = res.version_id;
    }

    /* dump tree */
    dump_tree(x, v);

    // printf("sync...\n");
    // struct timeval tv;
    // gettimeofday(&tv, NULL);
    // double total_seconds1 = (double)tv.tv_sec + (double)tv.tv_usec / 1000000.0;
    // 
    // tree_sync_and_free(x);
    // 
    // gettimeofday(&tv, NULL);
    // double total_seconds2 = (double)tv.tv_sec + (double)tv.tv_usec / 1000000.0;
    // printf("sync used %f seconds\n", total_seconds2 - total_seconds1);
    // // tree_free(x);x
    
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
        
        struct akinator_task_data task_data;

        task_data.dont_solved = 0;
        task_data.user = &user;
        task_data.iterator = tree_iterator_create(x, v);
        
        struct worker_task_id task = worker_pool_start_task(p, &task_data);

        while (p->free_id_len != p->tasks_alloc)
        {
            char *text = malloc(1000);
            printf("ANSWER: ");
            scanf("%s[^\n]\n", text);
            if (p->free_id_len != p->tasks_alloc)
            {
                worker_pool_receive_event(p, task, (void *)text);
            }
        }

        printf("Game ended\n");

        worker_pool_free(p);
    }
    
    return 0;
}
