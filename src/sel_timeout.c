#define DEBUG
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <arpa/inet.h>

#include "sel_timeout.h"
#include "config.h"
#include "client_info.h"
#include "dijkstra.h"
#include "dbg_helper.h"

#define ALREADY_TIMEOUT 1
#define NOT_TIMEOUT 0

static long g_adv_timeout;
static long adv_timeout_interval;

// flags indicating timeout 
static int is_adv_timeout;
static int is_lsa_timeout;
static int is_nb_timeout; // is neigbor down


extern hash_info_t g_lsa_ht;
extern hash_info_t g_nb_nodes_ht;
extern hash_info_t g_pd_ack_ht;
extern uint32_t g_nid;

void set_adv_timeout_interval(long interval)
{
    assert(interval > 0);
    adv_timeout_interval = interval;
    g_adv_timeout = adv_timeout_interval;
}

void reset_adv_time()
{
    g_adv_timeout = adv_timeout_interval;
}

static void preprocess_timeout(long time_elapsed)
{
    int i;
    lsa_t *lsa_node = NULL;
    is_lsa_timeout = NOT_TIMEOUT;
    is_adv_timeout = NOT_TIMEOUT;
    is_nb_timeout = NOT_TIMEOUT;
    node_info_t *nb;
    struct sockaddr_in addr;

    /* update adv timeout */
    g_adv_timeout -= time_elapsed;
    if(g_adv_timeout < 0)
    {
        is_adv_timeout = ALREADY_TIMEOUT;
    }

#ifdef DBG_TIME_OUT
    fprintf(stderr, "node %u adv remain time %ld\n",
            g_nid, g_adv_timeout / ONESEC);
#endif

    /* update nbr & LSA timeout*/
    for (i = 0; i < g_lsa_ht.bucket_size; i++)
    {   
        lsa_node = (lsa_t *)(g_lsa_ht.table_base[i]);
        for(; lsa_node != NULL; lsa_node = lsa_node->next)
        {
            /* ignore the LSA already invalidated, ignore local lsa */
            if ((lsa_node->is_valid == LSA_INVALID) || (lsa_node->snder_id == g_nid))
                continue;
            
            /* only less than 0 counts to timeout */
            lsa_node->time_left -= time_elapsed;

#ifdef DBG_TIME_OUT
            fprintf(stdout, "node %u LSA remain time %ld\n",
                    lsa_node->snder_id, lsa_node->time_left / ONESEC);
#endif
            if(lsa_node->time_left < 0)
            {
                lsa_node->is_valid = LSA_INVALID;
                is_lsa_timeout = ALREADY_TIMEOUT;
                pft_rm_obj_frm_lsa(lsa_node);
            
                HT_FIND(&g_nb_nodes_ht, node_info_t, nid, lsa_node->snder_id, nb);
                if(nb)  // neighbor down
                {
                    dbg_print(2, "nb id %u down\n", nb->nid);
                    inet_pton(AF_INET, nb->ip, &(addr.sin_addr)); 
                    addr.sin_port = htons(nb->rd_port);
                    addr.sin_family = AF_INET;

                    is_nb_timeout = ALREADY_TIMEOUT;
                    invalid_nb(nb);
                    clnt_rm_pd_ad_ann_by_addr(addr);
                }
            }
        }
    }

    return;
}

static void process_timeout(int rd_sock, fd_set *wrset)
{
    if(is_nb_timeout)
    {
        reload_local_lsa();
    }

    if(is_nb_timeout || is_adv_timeout)
    {
        clnt_prep_flood_lc_lsa(rd_sock);
        FD_SET(rd_sock, wrset);
        reset_adv_time();
    }

    if(is_lsa_timeout)
    {
        update_route_info();
    }
}

static void process_retran_timeout(int rd_sock, fd_set *wrset, long time_elapsed)
{
    int i;
    pend_lsa_t *pending_lsa = NULL;
    pend_lsa_t *pre_pd_lsa = NULL;
    pend_lsa_t *retran_lsa = NULL;
    
    for (i = 0; i < g_pd_ack_ht.bucket_size; i++)
    {   
        pending_lsa = (pend_lsa_t *)(g_pd_ack_ht.table_base[i]);
        pre_pd_lsa = NULL;

        while(pending_lsa != NULL)
        {
            pending_lsa->time_left -= time_elapsed;

            if (pending_lsa->time_left < 0)
            {
                dbg_print(1, "pending ad time out, to send to ip %s, port %d\n", inet_ntoa(pending_lsa->addr.sin_addr), ntohs(pending_lsa->addr.sin_port));
    
                /* ACK timeout, need to retransmit the LSA information */
                //detach the node
                if(pre_pd_lsa == NULL) // first one in the bucket
                {
                    g_pd_ack_ht.table_base[i] = pending_lsa->next;
                }
                else
                {
                    pre_pd_lsa->next = pending_lsa->next;
                }
                g_pd_ack_ht.entr_cnt--;

                retran_lsa = pending_lsa;

                pending_lsa = pending_lsa->next; 

                prep_retran_pkt(rd_sock, retran_lsa);
                FD_SET(rd_sock, wrset);
            }
            else
            {
                pre_pd_lsa = pending_lsa;
                pending_lsa = pending_lsa->next;
            }
        }
    }

    return;
}

void init_select_interval(struct timeval *tv, long check_interval)
{
    tv->tv_sec = 0;
    tv->tv_usec = check_interval;
    return;
}

int is_timeout(struct timeval *tv)
{
    /* interval is too short, no need to check timeout */
    if (tv->tv_usec > 0)
    {
        return 0;
    }
    else
    {
        return 1;
    }
}

void handle_time_out(int rd_sock, fd_set *wrset, long check_interval)
{
    long time_elapsed;
    

    /* check timeout and reset select interval */
    time_elapsed = check_interval;

    /* lsa, neighbor, adv timeout */
    preprocess_timeout(time_elapsed);
    process_timeout(rd_sock, wrset);

    /* retran timeout*/
    process_retran_timeout(rd_sock, wrset, time_elapsed);

    return;
}
