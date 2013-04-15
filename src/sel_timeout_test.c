#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include "select_timeout.h"
#include "config.h"

//typedef int size_t;
#define LSA_BUCKET_SIZE 100
#define NEIGHBOR_HT_BUCKET 100
#define PEND_ACK_BUCKET_SIZE 50

//extern void print_shortest_distance(int node_num, int d[]);
//extern void get_next_hop(int src, int dst, int prev[], int *next_hop);
//extern void dijkstra(int source, int node_num, int dist[], int d[], int prev[]);
extern void init_route_table();
//extern void print_route_table();
extern void update_route_info();

//extern route_table_t g_route_table;
uint32_t g_nid;

hash_info_t g_lsa_ht;
hash_info_t g_nb_nodes_ht;
hash_info_t g_pd_ack_ht;
static lsa_t *lsa_bucket[LSA_BUCKET_SIZE];
static node_info_t *nodeinf_bucket[NEIGHBOR_HT_BUCKET];
pend_lsa_t *pd_ack_bucket[PEND_ACK_BUCKET_SIZE];


size_t uint_to_idx(void *key, size_t bucket_size)
{
    return (size_t)key % bucket_size;
}

int cmp_int(const void *int1, const void *int2)
{
    if((long)int1 == (long)int2)
        return 0;

    return -1;
}

void init_nbr()
{
    g_nb_nodes_ht.table_base = (void **)nodeinf_bucket;
    g_nb_nodes_ht.bucket_size = NEIGHBOR_HT_BUCKET;
    g_nb_nodes_ht.key_to_idx = uint_to_idx; 
    g_nb_nodes_ht.cmp_key = cmp_int;
    g_nb_nodes_ht.entr_cnt = 0;

    return;
}


int init_lsa()
{
    g_lsa_ht.table_base = (void **)lsa_bucket;
    g_lsa_ht.bucket_size = LSA_BUCKET_SIZE;
    g_lsa_ht.key_to_idx = uint_to_idx;
    g_lsa_ht.cmp_key = cmp_int;  
    g_lsa_ht.entr_cnt = 0;

    return 0;
}

void init_pd_ack_table()
{
    g_pd_ack_ht.table_base = (void **)pd_ack_bucket;
    g_pd_ack_ht.bucket_size = PEND_ACK_BUCKET_SIZE;
    g_pd_ack_ht.key_to_idx = uint_to_idx;
    g_pd_ack_ht.cmp_key = cmp_int;
    g_pd_ack_ht.entr_cnt = 0;

    return;
}


/* this is only for test */
void create_lsa_entry(uint32_t sender_id, uint32_t nbr_id1, uint32_t nbr_id2, 
                      int is_valid)
{
    lnk_entr_t *lnk_base;
     
    uint32_t size = sizeof(lsa_t)
                    // need minus one for lnk num, to exclude the node itself
                    + 2 * sizeof(lnk_entr_t);

    lsa_t *llsa;
    char *p = (char *)malloc(size);

    llsa = (lsa_t *)p;
    llsa->next = NULL;
    llsa->is_valid = is_valid;
    llsa->ann_size = 1;

    llsa->ver = 1;
    llsa->ttl = LSA_MAX_TTL;
    llsa->snder_id = sender_id;
    llsa->seq_num = 0;
    llsa->num_lnk_entr = 2;
    llsa->num_obj_entr = 0;

    lnk_base = (lnk_entr_t *)(p + sizeof(lsa_t));
    lnk_base->nid = nbr_id1;
    lnk_base += 1;
    lnk_base->nid = nbr_id2;

    /* test */
    reset_LSA_timeout(llsa);
    
    HT_INSERT(&g_lsa_ht, lsa_t, llsa->snder_id, llsa);
    return;
}

void create_nbr_entry(uint32_t nbr_id)
{
    uint32_t size = sizeof(node_info_t);

    node_info_t *nbr_info;
    char *p = (char *)malloc(size);

    nbr_info = (node_info_t *)p;

    nbr_info->next = NULL;
    nbr_info->nid = nbr_id;
    nbr_info->rd_port = 0;
    nbr_info->local_port = 0;
    nbr_info->server_port = 0;
    //nbr_info->ip = "aa";
    nbr_info->is_valid = 1; 

    HT_INSERT(&g_nb_nodes_ht, node_info_t, nbr_info->nid, nbr_info);
    return;
}

void create_pend_lsa_entry(uint32_t nid, uint32_t seqno)
{
    uint32_t size = sizeof(pend_lsa_t);

    pend_lsa_t *pending_lsa;
    char *p = (char *)malloc(size);

    pending_lsa = (pend_lsa_t *)p;


    pending_lsa->next = NULL;
    pending_lsa->snder_id = nid;
    pending_lsa->seq_num = seqno;
    pending_lsa->type = LSA_TYPE_AD;  //ad, ack or seq

    reset_ACK_timeout(pending_lsa); 

    HT_INSERT(&g_pd_ack_ht, pend_lsa_t, nid, pending_lsa);
    return;
}


int main(int argc, char *argv[])
{
    int ret;
    struct timeval tv;

    init_lsa();
    init_nbr();
    init_pd_ack_table();
    
    reset_adv_timeout();
    g_nid = 11;
    
    create_lsa_entry(11, 12, 12, LSA_VALID);
    create_lsa_entry(12, 11, 11, LSA_VALID);
    create_lsa_entry(13, 11, 12, LSA_VALID);

    create_nbr_entry(12);

    create_pend_lsa_entry(11, 100);
    
#ifdef DBG_ROUTE_TABLE
    fprintf(stdout, "lsa entry cnt: %d\n", (int)g_lsa_ht.entr_cnt);
    fprintf(stdout, "pending entry cnt: %d\n", (int)g_pd_ack_ht.entr_cnt);
    fprintf(stdout, "nbr entry cnt: %d\n", (int)g_nb_nodes_ht.entr_cnt);
#endif

    init_select_interval(&tv);
    while(1)
    {   
        ret = select(0, NULL, NULL, NULL, &tv);
        handle_time_out(&tv);
    }

	return 0;
}
