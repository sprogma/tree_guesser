#define __USE_MINGW_ANSI_STDIO 1
#include "stdio.h"
#include "tree.h"

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
            printf("node VARIANT %lld [%p]: type: %d ptr_count: %d\n", node_id, node, node->type, node->ptr_count);
            print_tree(a, r_id, indent + 1);
        }
        else
        {
            PRINT_INDENT(indent);
            printf("node LEAF %lld [%p]: type: %d ptr_count: %d\n", node_id, node, node->type, node->ptr_count);
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

    /* insert node to tree */
    {
        struct tree_iterator *it = tree_iterator_create(x, v);
        struct tree_set_leaf_result res = tree_set_leaf(x, it, 1, NULL);
        printf("1 created: %lld %lld\n", res.version_id, res.new_node_id);
        v = res.version_id;
    }
    dump_tree(x, v);
    {
        struct tree_iterator *it = tree_iterator_create(x, v);
        struct tree_split_node_result res = tree_split_node(x, it, 1, NULL);
        printf("2 splittd: %lld %lld\n", res.version_id, res.new_node_id);
        v = res.version_id;
    }
    dump_tree(x, v);
    {
        struct tree_iterator *it = tree_iterator_create(x, v);
        struct tree_set_leaf_result res = tree_set_leaf(x, it, 0, NULL);
        printf("3 created: %lld %lld\n", res.version_id, res.new_node_id);
        v = res.version_id;
    }
    dump_tree(x, v);
    {
        struct tree_iterator *it = tree_iterator_create(x, v);
        struct tree_split_node_result res = tree_split_node(x, it, 1, NULL);
        printf("4 splittd: %lld %lld\n", res.version_id, res.new_node_id);
        v = res.version_id;
    }
    dump_tree(x, v);
    {
        struct tree_iterator *it = tree_iterator_create(x, v);
        tree_iterator_move_down(it, 1);
        struct tree_split_node_result res = tree_split_node(x, it, 0, NULL);
        printf("5 splittd: %lld %lld\n", res.version_id, res.new_node_id);
        v = res.version_id;
    }
    dump_tree(x, v);
    {
        struct tree_iterator *it = tree_iterator_create(x, v);
        tree_iterator_move_down(it, 1);
        struct tree_set_leaf_result res = tree_set_leaf(x, it, 1, NULL);
        printf("6 created: %lld %lld\n", res.version_id, res.new_node_id);
        v = res.version_id;
    }

    /* dump tree */
    dump_tree(x, v);
    
    tree_free(x);
    
    return 0;
}
