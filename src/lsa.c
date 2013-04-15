#define DEBUG
#include <stdlib.h>
#include <string.h>

#include "lsa.h"
#include "config.h"
#include "ht.h"
#include "dbg_helper.h"

#define LSA_BUCKET_SIZE 100

extern hash_info_t g_lfile_ht;
extern hash_info_t g_nb_nodes_ht;
extern uint32_t g_nid;

hash_info_t g_lsa_ht; // lsa hash table
static lsa_t *lsa_bucket[LSA_BUCKET_SIZE];

static long lsa_timeout_interval;
static long nb_timeout_interval;
static long retran_timeout_interval;

/*
   Create local lsa based on obj and neighbor info
   Only valid (not down) neighbor will be added to the lsa
 */
static lsa_t *init_local_lsa(uint32_t seq_num)
{
    size_t nb_cnt = 0;
    size_t i;
    node_info_t *ninf;
    file_entry_t *obj_inf; 
    size_t cnt;
    lnk_entr_t *lnk_base;
    obj_entr_t *obj_base;

    // get total valid neighbor cnt 
    for(i = 0; i < g_nb_nodes_ht.bucket_size; i++)
    {
        ninf = ((node_info_t **)g_nb_nodes_ht.table_base)[i];
        while(ninf)
        {
             // need exclude self and down nb
            if(ninf->nid != g_nid && ninf->is_valid)
            {
                nb_cnt++;
            }
            ninf = ninf->next;
        }
    } 

    uint32_t size = sizeof(lsa_t)
                    + nb_cnt * sizeof(lnk_entr_t)
                    + g_lfile_ht.entr_cnt * sizeof(obj_entr_t);

    lsa_t *llsa;
    char *p = (char *)malloc(size);
    if(!p)
        goto err;

    memset(p, 0, size);

    llsa = (lsa_t *)p;
    llsa->next = NULL;
    llsa->is_valid = LSA_VALID;
    llsa->time_left = lsa_timeout_interval;
    llsa->ann_size = sizeof(lsa_header_t)
                     + nb_cnt * sizeof(lnk_entr_t)
                     + g_lfile_ht.entr_cnt * sizeof(obj_entr_t);

    llsa->ver = 1;
    llsa->ttl = LSA_MAX_TTL;
    llsa->type = LSA_TYPE_AD;
    llsa->snder_id = g_nid;
    llsa->seq_num = seq_num;
    llsa->num_lnk_entr = nb_cnt;
    llsa->num_obj_entr = g_lfile_ht.entr_cnt;

    lnk_base = (lnk_entr_t *)(p + sizeof(lsa_t));
    obj_base = (obj_entr_t *)(p + sizeof(lsa_t)
                              + llsa->num_lnk_entr * sizeof(lnk_entr_t));

    // create link entry
    cnt = 0;
    for(i = 0; i < g_nb_nodes_ht.bucket_size; i++)
    {
        ninf = ((node_info_t **)g_nb_nodes_ht.table_base)[i];
        while(ninf)
        {
            if(ninf->nid != g_nid && ninf->is_valid)
            {
                lnk_base[cnt].nid = ninf->nid;
                cnt++;
            }

            ninf = ninf->next;
        }

    }

    //create obj entries
    cnt = 0;
    for(i = 0; i < g_lfile_ht.bucket_size; i++)
    {
        obj_inf = ((file_entry_t **)g_lfile_ht.table_base)[i];
        while(obj_inf)
        {
            strcpy(obj_base[cnt].obj, obj_inf->file_name);
            cnt++;
            obj_inf = obj_inf->next;
        }
    }


    HT_INSERT(&g_lsa_ht, lsa_t, llsa->snder_id, llsa);
    
    return llsa;
err:
    if(p)
        free(p);

    return NULL;
}


// -1 for failure
lsa_t *init_lsa()
{
    // init hash table 
    g_lsa_ht.table_base = (void **)lsa_bucket;
    g_lsa_ht.bucket_size = LSA_BUCKET_SIZE;
    g_lsa_ht.key_to_idx = uint_to_idx;
    g_lsa_ht.cmp_key = cmp_int;  
    g_lsa_ht.entr_cnt = 0;


    /* create local lsa */
    return init_local_lsa(0);
} 

/* 
   Reload the local lsa based on the current neighbor state
   and obj info. The sequence number will remains the same
 */
lsa_t *reload_local_lsa()
{
    lsa_t *old_lsa;
    lsa_t *new_lsa;
    HT_FIND(&g_lsa_ht, lsa_t, snder_id, g_nid, old_lsa);
    assert(old_lsa);
    HT_REMOVE(&g_lsa_ht, lsa_t, snder_id, g_nid);

    if(NULL == (new_lsa = init_local_lsa(old_lsa->seq_num)))
    {
        // failed to generate new lsa, roll back to old one
        HT_INSERT(&g_lsa_ht, lsa_t, old_lsa->snder_id, old_lsa);
        return NULL;
    }
    else
    {
        free(old_lsa);
        return new_lsa;
    }

}

/*
   Update an lsa entry based on the lsa head and body provided
 */
lsa_t *update_lsa_entr(lsa_header_t *head, char *body, size_t body_sz)
{
    lsa_t *old_lsa;
    lsa_t *new_lsa;
    node_info_t *nb;
    
    new_lsa = (lsa_t *)malloc(sizeof(lsa_t) + body_sz);
    if(!new_lsa)
        return NULL;
    
    HT_FIND(&g_lsa_ht, lsa_t, snder_id, head->snder_id, old_lsa);
    if(old_lsa)
    {
        HT_REMOVE(&g_lsa_ht, lsa_t, snder_id, head->snder_id);
        free(old_lsa);
    }

    HT_FIND(&g_nb_nodes_ht, node_info_t, nid, head->snder_id, nb);

    new_lsa->next = NULL;
    new_lsa->is_valid = LSA_VALID;
   
    if(nb)
    {
        new_lsa->time_left = nb_timeout_interval; 
    } 
    else
    {
        new_lsa->time_left = lsa_timeout_interval; 
    }

    new_lsa->ann_size = sizeof(lsa_header_t) + body_sz;

    new_lsa->ver = head->ver;
    new_lsa->ttl = head->ttl - 1;
    new_lsa->type = LSA_TYPE_AD;
    new_lsa->snder_id = head->snder_id;
    new_lsa->seq_num = head->seq_num;
    new_lsa->num_lnk_entr = head->num_lnk_entr;
    new_lsa->num_obj_entr = head->num_obj_entr;
    
    memcpy((char *)new_lsa + sizeof(lsa_t), body, body_sz);

    HT_INSERT(&g_lsa_ht, lsa_t, new_lsa->snder_id, new_lsa); 


    dbg_print_stored_lsa(new_lsa);

    return new_lsa;
}

/*
   Try to update the sequence number of an lsa, if and only if the 
   seq num provided if larger than the one currently stored in the lsa
 */
int update_seq_num(uint32_t snder_id, uint32_t seq_num)
{
    lsa_t *llsa;

    HT_FIND(&g_lsa_ht, lsa_t, snder_id, snder_id, llsa);
    if(llsa && llsa->seq_num < seq_num)
    {
        llsa->seq_num = seq_num;
        dbg_print(0, "nid %u seq_num after update is %u\n", llsa->snder_id, llsa->seq_num);
        return 1;
    }
    return 0;
}

pend_lsa_t *lsa_create_ack_ann(lsa_header_t *head, struct sockaddr_in addr)
{
    pend_lsa_t *pd_lsa;

    pd_lsa = malloc(sizeof(pend_lsa_t) - 1 + sizeof(lsa_header_t));
    if(pd_lsa)
    {
        pd_lsa->next = NULL;
        pd_lsa->addr = addr;
        pd_lsa->type = LSA_TYPE_ACK;

        head->type = LSA_TYPE_ACK;
        head->ttl = 1;
        // head->seq_num unchanged
        head->num_lnk_entr = 0;
        head->num_obj_entr = 0;

        pd_lsa->payload_size = sizeof(lsa_header_t);
        memcpy(pd_lsa->payload, head, sizeof(lsa_header_t));

    }

    return pd_lsa;
}

pend_lsa_t *lsa_create_seq_ann(lsa_header_t *head, uint32_t seq_num, 
                           struct sockaddr_in addr)
{
    pend_lsa_t *pd_lsa;
    pd_lsa = malloc(sizeof(pend_lsa_t) - 1 + sizeof(lsa_header_t));
    if(pd_lsa)
    {
        pd_lsa->next = NULL;
        pd_lsa->addr = addr;
        pd_lsa->type = LSA_TYPE_SEQ;

        head->type = LSA_TYPE_SEQ;
        head->ttl = 1;
        head->seq_num = seq_num; 
        head->num_lnk_entr = 0;
        head->num_obj_entr = 0;

        pd_lsa->payload_size = sizeof(lsa_header_t);
        memcpy(pd_lsa->payload, head, sizeof(lsa_header_t));
    }
    return pd_lsa;
}

pend_lsa_t *lsa_create_ad_ann(lsa_t *lsa, struct sockaddr_in addr)
{
    pend_lsa_t *pd_lsa;

    pd_lsa = malloc(sizeof(pend_lsa_t) - 1 + lsa->ann_size);
    if(pd_lsa)
    {
        pd_lsa->next = NULL;
        
        pd_lsa->snder_id = lsa->snder_id;
        pd_lsa->seq_num = lsa->seq_num;
        pd_lsa->addr = addr;
        pd_lsa->time_left = retran_timeout_interval;
        pd_lsa->is_acked = 0;
        pd_lsa->type = LSA_TYPE_AD; 

        pd_lsa->payload_size = lsa->ann_size;
        memcpy(pd_lsa->payload, &(lsa->ver), lsa->ann_size);
    }
    return pd_lsa; 
}

void lsa_update_retran_timeout(pend_lsa_t *pd_lsa)
{
    pd_lsa->time_left = retran_timeout_interval;
}

void set_retran_timeout_interval(long interval)
{
    assert(interval > 0);
    retran_timeout_interval = interval;
}

void set_nb_timeout_interval(long interval)
{
    assert(interval > 0);
    nb_timeout_interval = interval;
}

void set_lsa_timeout_interval(long interval)
{
    assert(interval > 0);
    lsa_timeout_interval = interval;    
}

void dbg_print_stored_lsa(lsa_t *lsa_h)
{
    obj_entr_t *obj_base;
    lnk_entr_t *lnk_base;

    fprintf(stderr, "*********************stored lsa*********************\n");
    fprintf(stderr, "ver %hhu  ", lsa_h->ver);
    fprintf(stderr, "ttl %hhu  ", lsa_h->ttl);
    fprintf(stderr, "type %hu  ", lsa_h->type); 
    fprintf(stderr, "snder id %u  ", lsa_h->snder_id);
    fprintf(stderr, "seq_num %u  ", lsa_h->seq_num); 
    fprintf(stderr, "num lnk %u  ", lsa_h->num_lnk_entr); 
    fprintf(stderr, "num obj %u\n", lsa_h->num_obj_entr); 

    lnk_base = (lnk_entr_t *)((char *)lsa_h + sizeof(lsa_t));
    obj_base = (obj_entr_t *)((char *)lsa_h + sizeof(lsa_t)
                + lsa_h->num_lnk_entr * sizeof(lnk_entr_t));

    int i;
    for(i = 0; i< lsa_h->num_lnk_entr; i++)
    {
        fprintf(stderr, "%u\n", lnk_base[i].nid);
    }

    for(i = 0; i< lsa_h->num_obj_entr; i++)
    {
        fprintf(stderr, "%s\n", obj_base[i].obj);
    }
    fprintf(stderr, "*********************************************\n");
}
