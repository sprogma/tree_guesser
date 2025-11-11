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
    int32_t type; /* enum node_type */
    
    /* not used for now */
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
    struct record *record;    
};

#define MAX_NODE_SIZE ((int64_t)(sizeof(struct node_leaf) > sizeof(struct node_variant) ? sizeof(struct node_leaf) : sizeof(struct node_variant)))
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

    HANDLE pagefile;

    _Atomic int64_t access_token;

    SRWLOCK lock;
};


/* ! iterators aren't thread safe structures */
struct tree_iterator
{
    struct tree *tree;
    int64_t version_id;
    int64_t *path;
    int64_t path_len;
    int64_t path_alloc;
};




struct node_allocator *allocator_create(const char *db_filename);
void allocator_sync_and_free(struct node_allocator *allocator);
void allocator_free(struct node_allocator *allocator);


struct node *allocator_acquire_node(struct node_allocator *allocator, int64_t node_id, int32_t exclusive);

void allocator_release_node(struct node_allocator *allocator, struct node *node, int32_t exclusive);

/* use tree_release_version if possible, becouse it is faster */
void allocator_release_node_by_id(struct node_allocator *allocator, int64_t node_id, int32_t exclusive);

#define NODE_WAS_MODITIFIED 0x179
#define NODE_ALREADY_UNLOADED 0x998
#define NODE_WAS_FREED 0x244
#define NODE_IS_FREE 0x353
int32_t allocator_try_unload_node(struct node_allocator *allocator, int64_t node_id);
void allocator_unload_node_force(struct node_allocator *allocator, int64_t node_id);


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
void node_make_copy(struct node *dest_bs, struct node *node_bs);


/* check if node are equal */
int32_t node_is_isomorphic(struct node *node_a, struct node *node_b);

/* returns node size in bytes */
int64_t node_get_size(struct node *node);


struct tree *tree_create();
void tree_sync_and_free(struct tree *tree);
void tree_free(struct tree *tree);



struct tree_version *tree_acquire_version(struct tree *tree, int64_t version_id, int32_t exclusive);

void tree_release_version(struct tree *tree, struct tree_version *version, int32_t exclusive);

/* use tree_release_version if possible, becouse it is faster */
void tree_release_version_by_id(struct tree *tree, int64_t version_id, int32_t exclusive);



/* ! iterators aren't thread safe structures */
struct tree_iterator *tree_iterator_create(struct tree *tree, int64_t version_id);

/* shift_from_current is shift from last node with direction at tree root:
   shift = 0 means current node, shift = 1 - it's parent, and so on */
int64_t tree_iterator_get_node(struct tree_iterator *iterator, int64_t shift_from_current);
/* return depth of current element */
int64_t tree_iterator_get_depth(struct tree_iterator *iterator);

void tree_iterator_move_up(struct tree_iterator *iterator);
void tree_iterator_move_down(struct tree_iterator *iterator, int32_t move_to_left);

void tree_iterator_free(struct tree_iterator *iterator);


struct node_allocator *tree_get_allocator(struct tree *tree);




struct tree_split_node_result {
    int64_t version_id;
    int64_t new_node_id;
};
struct tree_split_node_result tree_split_node(struct tree *tree, struct tree_iterator *iterator, int32_t node_is_now_left, struct question *question);




struct tree_set_leaf_result {
    int64_t version_id;
    int64_t new_node_id;
};
struct tree_set_leaf_result tree_set_leaf(struct tree *tree, struct tree_iterator *iterator, int32_t set_as_left_child, struct record *record);





#endif
