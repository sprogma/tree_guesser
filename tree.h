#ifndef TREE_H
#define TREE_H

#ifdef _WIN32_WINNT
    #undef _WIN32_WINNT
#endif
#define _WIN32_WINNT 0x0601
#include "windows.h"

#include "inttypes.h"

struct quesion
{
    char *text;
};


struct record
{
    char *name;
};


enum node_type
{
    NODE_VARIANT,
    NODE_LEAF,
};

struct node
{
    enum node_type type;
    _Atomic int32_t ptr_count;
    _Atomic int64_t last_access;
    SRWLOCK lock;
};


struct node_variant
{
    struct node;
    int64_t l, r;
    struct question *question;
};


struct node_leaf
{
    struct node;
    struct record *guess;    
};


struct tree_version
{
    int64_t *root;
    int64_t size;
    int64_t parent;
    /* for searching LCA */
    int64_t far_parent;
    int64_t depth;
};


struct tree
{
    struct tree_version *versions;
    int64_t              versions_len;
    int64_t              versions_alloc;
    
    SRWLOCK lock;

    struct node_allocator *allocator;
};


struct akinator
{
    struct tree *tree;
};


#define UNLOADED_NODE ((void *)(1))
#define INVALID_NODE_ID (-1)


struct node_allocator
{
    struct node **nodes;
    int64_t nodes_len;
    int64_t nodes_alloc;
    
    int64_t loaded_nodes;

    _Atomic int64_t access_token;

    SRWLOCK lock;
};






struct node_allocator *allocator_create();
void allocator_free(struct node_allocator *allocator);
struct node *allocator_acquire_node(struct node_allocator *allocator, int64_t node_id, int32_t exclusive);
void allocator_release_node(struct node_allocator *allocator, int64_t node_id, int32_t exclusive);
int64_t allocator_create_node(struct node_allocator *allocator, enum node_type type);

/* this is inner function, don't use it directly */
/* result node will be locked with exclusive access */
struct node *node_create();


struct tree *tree_create();
void tree_free(struct tree *tree);






#endif
