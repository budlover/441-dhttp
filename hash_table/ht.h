#ifndef _HT_H_
#define _HT_H_
#include <assert.h>

typedef struct hash_inf
{
    void **table_base; 
    size_t bucket_size;
    size_t (*key_to_idx)(void *key, size_t bucket_size);
    int (*cmp_key)(const void *key_i, const void *key);
} hash_info_t;

#define HT_INSERT(HASH_INFO, STRUCT_TYPE, INSERT_KEY, NODE)   \
do{\
    STRUCT_TYPE **table = (STRUCT_TYPE **)(HASH_INFO)->table_base;\
    size_t idx = (HASH_INFO)->key_to_idx((void *)(long)INSERT_KEY, (HASH_INFO)->bucket_size); \
    STRUCT_TYPE *node = table[idx]; \
    table[idx] = NODE;\
    (NODE)->next = node;\
}while(0)

#define HT_FIND(HASH_INFO, STRUCT_TYPE, KEY_NAME, TARGET_KEY, P) \
do{\
    STRUCT_TYPE **table = (STRUCT_TYPE **)(HASH_INFO)->table_base;\
    size_t idx = (HASH_INFO)->key_to_idx((void *)(long)TARGET_KEY, (HASH_INFO)->bucket_size); \
    STRUCT_TYPE *node = table[idx]; \
    while(node)\
    {\
        if(!(HASH_INFO)->cmp_key((void *)(long)node->KEY_NAME, (void *)(long)TARGET_KEY))\
            break;\
        else\
            node = node->next;\
    }\
    P = node;\
}while(0)


/*
    User is responsible to free the node
*/
#define HT_REMOVE(HASH_INFO, STRUCT_TYPE, KEY_NAME, TARGET_KEY)\
do{\
    STRUCT_TYPE **table = (STRUCT_TYPE **)(HASH_INFO)->table_base;\
    size_t idx = (HASH_INFO)->key_to_idx((void *)(long)TARGET_KEY, (HASH_INFO)->bucket_size); \
    STRUCT_TYPE *curr = table[idx]; \
    STRUCT_TYPE **pre_next = table + idx; \
    while(curr)\
    {\
        if(!(HASH_INFO)->cmp_key((void *)(long)curr->KEY_NAME, (void *)(long)TARGET_KEY))\
            break;\
        else\
        {\
            pre_next = &curr->next;\
            curr = curr->next;\
        }\
    }\
    assert(curr);\
    *pre_next = curr->next;\
}while(0)


#endif
