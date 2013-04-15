#include <stdlib.h>
#include <stdio.h>
#include <memory.h>
#include <assert.h>
#include <time.h>

#include "ht.h"
#define TEST_NUM 999
#define KEY_LEN 19

typedef struct node
{
    char key[KEY_LEN + 1];
    struct node *next;
} node_t;


node_t *test_nodes[TEST_NUM];

size_t str_to_idx(void *key, size_t bucket_size)
{
    size_t sum = 0;
    char *c = key;
    while(*c)
    {
        sum += *c;
        c++;
    }
    sum = sum % bucket_size;

    return sum;
}

int cmpstr(const void *str1, const void *str2)
{
    return strcmp(str1, str2);
}

int cmp_int(const void *int1, const void *int2)
{
    if((long)int1 == (long)int2)
        return 0;

    return -1;
} 

int main()
{
    node_t *arr[100];
    memset(arr, 0, sizeof(node_t *) * 100);

    hash_info_t ht;
    ht.table_base = (void **)arr;
    ht.bucket_size = 100;
    ht.key_to_idx = str_to_idx;
    ht.cmp_key = cmpstr;

    srand(time(NULL));

    char tmp[KEY_LEN + 1];

    int i; 
    //test insert
    for(i = 0; i < TEST_NUM; i++)
    {
        test_nodes[i] = (node_t *)malloc(sizeof(node_t));
        assert(test_nodes[i]);
        sprintf(test_nodes[i]->key, "%d", i);
        test_nodes[i]->next = NULL;
        HT_INSERT(&ht, node_t, test_nodes[i]->key, test_nodes[i]);
    }
   

    //test find 
    for(i = 0; i< TEST_NUM; i++)
    {
        node_t *p;
        sprintf(tmp, "%d", i);
        HT_FIND(&ht, node_t, key, tmp, p);
        assert(p == test_nodes[i]);
    }
  
   //test remove 
    int cnt = TEST_NUM; 
    for(i = 0; i < TEST_NUM / 2; i++)
    {
        int rd = rand() % TEST_NUM;
        if(!test_nodes[rd])
            continue;
        cnt--;
        sprintf(tmp, "%d", rd);
        HT_REMOVE(&ht, node_t, key, tmp); 
        free(test_nodes[rd]);
        test_nodes[rd] = NULL;

    }

    int rem_cnt = 0;
    for(i = 0; i< TEST_NUM; i++)
    {
        node_t *p;
        sprintf(tmp, "%d", i);
        HT_FIND(&ht, node_t, key, tmp, p);
        if(!p)
        {
            rem_cnt++;
        }
        assert(p == test_nodes[i]);
        if(p)
        {
//            printf("num is %d, inside key is %s\n", i, p->key);
        }
    }

    printf("Successfully\n left %d remove %d original %d\n", cnt, rem_cnt, TEST_NUM);
    return 0;
}

