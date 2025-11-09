#define __USE_MINGW_ANSI_STDIO 1
#include "stdio.h"
#include "tree.h"


int main()
{

    struct node_allocator *x = allocator_create();
    
    printf("x = %p\n", x);

    int64_t a = allocator_create_node(x, NODE_LEAF);
    int64_t b = allocator_create_node(x, NODE_LEAF);

    printf("a = %lld\n", a);
    printf("b = %lld\n", b);

    struct record guess = {NULL};

    struct node_leaf *res;
    
    res = (struct node_leaf *)allocator_acquire_node(x, a, 0);
    printf("a->guess was %p\n", res->guess);
    allocator_release_node(x, a, 0);
    
    res = (struct node_leaf *)allocator_acquire_node(x, a, 1);
    res->guess = &guess;
    printf("a->guess now %p\n", res->guess);
    allocator_release_node(x, a, 1);
    
    allocator_free(x);
    return 0;
}
