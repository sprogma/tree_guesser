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
    int64_t parent;
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
    struct record *record;    
};


#define UNLOADED_NODE ((void *)(1))
#define INVALID_NODE_ID (-1)

struct tree_version
{
    int64_t root;
    int64_t size;
    int64_t parent;
    /* for searching LCA */
    int64_t far_parent;
    int64_t depth;

    SRWLOCK lock;
};


struct tree
{
    struct tree_version **versions;
    int64_t               versions_len;
    int64_t               versions_alloc;
    
    SRWLOCK lock;

    struct node_allocator *allocator;
};


struct akinator
{
    struct tree *tree;
};


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

/* this is inner function, don't use it directly */
/* result node will be locked with exclusive access */
struct allocator_create_node_result {
    int64_t node_id;
    struct node *node;
};
struct allocator_create_node_result allocator_create_node(struct node_allocator *allocator, enum node_type type);

/* this is inner function, don't use it directly */
/* result node will be locked with exclusive access */
struct node *node_create();

/* this is inner function, don't use it directly */
void node_copy(struct node *dest_bs, struct node *node_bs);


struct tree *tree_create();
void tree_free(struct tree *tree);

struct tree_split_node_result {
    int64_t version_id;
    int64_t new_node_id;
};
struct tree_split_node_result tree_split_node(struct tree *tree, int64_t version, int64_t node_id, int32_t node_is_now_left, struct question *quesion);

struct tree_set_leaf_result {
    int64_t version_id;
    int64_t new_node_id;
};
struct tree_set_leaf_result tree_set_leaf(struct tree *tree, int64_t version, int64_t node_id, struct record *record);


#endif
