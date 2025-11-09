#define __USE_MINGW_ANSI_STDIO 1
#include "stdio.h"
#include "tree.h"


int main()
{

    struct tree *x = tree_create();

    /* insert node to tree */
    struct tree_split_node_result res = tree_split_node(x, 0, -1, 1, NULL);
    printf("created: %lld %lld\n", res.version_id, res.new_node_id);
    
    tree_free(x);
    
    return 0;
}
