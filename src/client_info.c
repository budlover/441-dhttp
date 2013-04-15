#define DEBUG

#include <unistd.h>
#include <memory.h>
#include <assert.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "dbg_helper.h"
#include "client_info.h"
#include "forward.h"
#include "config.h"
#include "resp.h"
#include "config.h"
#include "ht.h"
#include "lsa.h"
#include "dijkstra.h"
#include "sel_timeout.h"

#define PEND_ACK_BUCKET_SIZE 50


// helper functions
static char *alloc_lsa_buff(lsa_header_t *head, size_t *size);
static int prep_ack_pkt(lsa_header_t *head, struct sockaddr_in addr, int fd);
static int prep_seq_pkt(lsa_header_t *head, uint32_t seq_num, 
                        struct sockaddr_in addr, int fd);
static int prep_flood_lsa(uint32_t snder_id, struct sockaddr_in excpt_addr, 
                          int rd_sock);


static client_info_t clnt_set[FD_SETSIZE];

extern hash_info_t g_lsa_ht;
extern hash_info_t g_lfile_ht;
extern hash_info_t g_nb_nodes_ht;
extern uint32_t g_nid;
extern uint32_t g_rd_port;

hash_info_t g_pd_ack_ht;
static pend_lsa_t *pd_ack_bucket[PEND_ACK_BUCKET_SIZE];
void dbg_print_lsa(char *buff);

void init_pd_ack_table()
{
    g_pd_ack_ht.table_base = (void **)pd_ack_bucket;
    g_pd_ack_ht.bucket_size = PEND_ACK_BUCKET_SIZE;
    g_pd_ack_ht.key_to_idx = uint_to_idx;
    g_pd_ack_ht.cmp_key = cmp_int;
    g_pd_ack_ht.entr_cnt = 0;
}

/*
   Init the struct for record info for an sock
 */
int create_clnt_info(int fd, SOCKET_TYPE type)
{
    assert(fd < FD_SETSIZE);
    buf_chain_t *p = NULL;
    clnt_set[fd].sock_type = type;
    char *send_buf; 
    
    //init recv buff 
    p = new_buf(); 
    if(!p)
        return -1;
    clnt_set[fd].first_buf = p;
    clnt_set[fd].curr_buf = p;

    //init send buff
    send_buf = malloc(SEND_BUFF_LEN);
    if(!send_buf)
        goto err;
    clnt_set[fd].send_buf = send_buf;

    // init parser and req_quest info record
    init_parser(p, &clnt_set[fd].parser_inf);
    init_req_info(&clnt_set[fd].req_inf);

    clnt_set[fd].pd_lsa_head = NULL;

    clnt_set[fd].is_exist = 1; //must be the last one incase error
    return 0;

err:
    free_buf(&clnt_set[fd].first_buf, NULL);
    clnt_set[fd].curr_buf = NULL;
    clnt_set[fd].first_buf = NULL;

    if(send_buf)
    {
        free(send_buf);
        clnt_set[fd].send_buf = NULL;
    }
    return -1;
}

/*
   Reset the struct for record info for an sock.
   All resource allocated for the sock will be reclaimed.
 */
void remove_clnt_info(int fd)
{
    assert(fd < FD_SETSIZE);
    assert(clnt_set[fd].is_exist != 0);
  
    //free all recv buff
    free_buf(&clnt_set[fd].first_buf, NULL);
    clnt_set[fd].curr_buf = NULL;
    clnt_set[fd].first_buf = NULL;

    if(clnt_set[fd].send_buf)
    {
        free(clnt_set[fd].send_buf);
        clnt_set[fd].send_buf = NULL;
    } 

    clnt_set[fd].is_exist = 0;
}

// get next available file descritor in all exist file descriptor set
// if no available one, return -1. otherwise reutrn the descriptor
int get_next_fd(fd_set *fdset)
{
    static int pos = 0;
    int ret;
    for(; pos < FD_SETSIZE; pos++) {
        if (clnt_set[pos].is_exist != 0 && FD_ISSET(pos, fdset)) {
            ret = pos;
            pos++;
            return ret;
        } 
    }
    
    pos = 0;    
    return -1;
}

client_info_t *get_clnt_info(int fd)
{
    assert(fd < FD_SETSIZE);
    return &clnt_set[fd];
}

/*
   Update all struct for a sock.
   This function is used only for struct recording TCP type sock 
 */
void update_clnt_info(int fd)
{
    assert(fd < FD_SETSIZE);
    update_parser_info(&clnt_set[fd].parser_inf);
    update_req_info(&clnt_set[fd].req_inf);
    //free the buffers that have all ready passed
    free_buf(&clnt_set[fd].first_buf, clnt_set[fd].curr_buf);
}

/*
    Wrapper function for receive data from TCP socket.
    The buffer chain may be extend if the current buffer is full
 */
int recv_clnt_data(int fd)
{
    assert(fd < FD_SETSIZE);
    int retv;
    buf_chain_t *curr_buf = clnt_set[fd].curr_buf;
    retv = recv(fd, curr_buf->payload + curr_buf->len, 
                   BUFBLK_SIZE - curr_buf->len,
                   MSG_DONTWAIT);
    if(retv > 0)
    {
        dbg_print(0, "%s:%.*s\n", __FUNCTION__, retv, curr_buf->payload + curr_buf->len);
        curr_buf->len += retv;
        // extend current buffer
        if(curr_buf->len == BUFBLK_SIZE)
            extend_buf(&clnt_set[fd].curr_buf);
    }

    return retv;
}

/*
   Parse the request and see if there exists a full request.
   Return 1 if gets a full request.
   Return 0 otherwise.
 */
int get_clnt_full_req(int fd)
{
    assert(fd < FD_SETSIZE);
    int retv;
    retv = parse_req(&clnt_set[fd].parser_inf, &clnt_set[fd].req_inf);
    if(retv) //new full request
    {
        dbg_print(0, "IN %s:method %d len1 %d field1 %s\n", 
                  __FUNCTION__,
                  clnt_set[fd].req_inf.method,
                  clnt_set[fd].req_inf.field1_len, 
                  clnt_set[fd].req_inf.field1);
    }

    return retv; 
}

/*
    Response to request.
    Lookup the obj for GETRD request
    Add obj file mapping for ADDFILE request
 */
int process_clnt_request(int fd, int *need_flood, int rd_sock)
{
    assert(fd < FD_SETSIZE);
    
    char ip[IP_LEN + 1];
    char local_file_path[MAX_FILE_PATH + 1];
    int server_port;
    int remote_lc_port;
    int ret;
    client_info_t *clnt_inf;

    clnt_inf = &clnt_set[fd];
    *need_flood = 0;

    if(clnt_inf->req_inf.status == REQUEST_STATUS_OK)
    {
        if(clnt_inf->req_inf.method == REQUEST_METHOD_GETRD)
        {
           ret = lookup_file(clnt_inf->req_inf.field1, ip, &server_port, 
                              &remote_lc_port, local_file_path);
            if(ret == -1)
            {
                clnt_inf->req_inf.status = REQUEST_STATUS_ERROR_NOT_FOUND;
            }
        }
        else  //ADDFILE
        {
            ret = add_localfile(clnt_inf->req_inf.field1, 
                                clnt_inf->req_inf.field2);
            if(ret == -1)
            {
                clnt_inf->req_inf.status = REQUEST_STATUS_ERROR_SERVER_INTERNAL;
            }
            else if(ret == ENTRY_EXISTS)
            {
                clnt_inf->req_inf.status = REQUEST_STATUS_ERROR_RECORD_EXISTS;
            }
            else
            {
                lsa_t *new_lsa;
                if(NULL != (new_lsa = reload_local_lsa()))
                {
                    pft_add_obj_frm_lsa(new_lsa);
                    clnt_prep_flood_lc_lsa(rd_sock);
                    reset_adv_time();
                    *need_flood = 1;
                }
            } 
        }
    }

    ret = fill_local_resp(clnt_inf->send_buf, SEND_BUFF_LEN, 
                          clnt_inf->req_inf.field1,
                          ip, server_port, remote_lc_port, local_file_path,
                          clnt_inf->req_inf.method, clnt_inf->req_inf.status);
    clnt_inf->send_len = ret;
    dbg_print(0, "response is:\n%.*s\n", ret, clnt_inf->send_buf);

    return 0;
}

int send_clnt_resp(int fd)
{
    assert(fd < FD_SETSIZE);
    send(fd, clnt_set[fd].send_buf, clnt_set[fd].send_len, MSG_DONTWAIT);
    return 0;
}

int need_close_clnt(int fd)
{
    assert(fd < FD_SETSIZE);

    if(clnt_set[fd].req_inf.status == REQUEST_STATUS_ERROR_BAD_REQUEST)
        return 1;
    else
        return 0;
}



int clnt_send_rd_info(int fd)
{
    assert(fd < FD_SETSIZE);
    assert(clnt_set[fd].sock_type == SOCKET_TYPE_UDP);
    
    socklen_t sklen = sizeof(struct sockaddr_in);

    pend_lsa_t *pd_lsa = clnt_set[fd].pd_lsa_head;

    while(pd_lsa)
    {
        int tmp;
        tmp = sendto(fd, pd_lsa->payload, pd_lsa->payload_size, 
                        MSG_DONTWAIT,
                       (struct sockaddr *)&pd_lsa->addr, sklen);
        if(-1 == tmp)
        {
                    perror("what happen when udp send");
        }
   
        //must detach before add to pd_ack_ht     
        clnt_set[fd].pd_lsa_head = pd_lsa->next;

        dbg_print(1, "send to %s %d  packet type is %d\n", inet_ntoa(pd_lsa->addr.sin_addr), ntohs(pd_lsa->addr.sin_port), pd_lsa->type);
        // only insert to pending hash table if the type is ad
        // ack and seq type don't acked
        if(pd_lsa->type == LSA_TYPE_AD)
        {
            HT_INSERT(&g_pd_ack_ht, pend_lsa_t, pd_lsa->snder_id, pd_lsa);
        }
        else
        {
            free(pd_lsa);
        }
        pd_lsa = clnt_set[fd].pd_lsa_head;
    }

    return 0;
}

void clnt_rm_pd_ad_ann_by_addr(struct sockaddr_in addr)
{
    int i;
    pend_lsa_t *pd_lsa = NULL;
    pend_lsa_t *pre_pd_lsa = NULL;
    pend_lsa_t *to_free_lsa = NULL;
    
    for (i = 0; i < g_pd_ack_ht.bucket_size; i++)
    {   
        pd_lsa = (pend_lsa_t *)(g_pd_ack_ht.table_base[i]);
        pre_pd_lsa = NULL;

        while(pd_lsa != NULL)
        {
            if (0 == memcmp(&addr, &pd_lsa->addr, 
                             sizeof(struct sockaddr_in) - 8))
            {
                dbg_print(1, "rm pending ad time out, to send to ip %s, port %d\n", inet_ntoa(pd_lsa->addr.sin_addr), ntohs(pd_lsa->addr.sin_port));
    
                //detach the node
                if(pre_pd_lsa == NULL) // first one in the bucket
                {
                    g_pd_ack_ht.table_base[i] = pd_lsa->next;
                }
                else
                {
                    pre_pd_lsa->next = pd_lsa->next;
                }
                g_pd_ack_ht.entr_cnt--;
                to_free_lsa = pd_lsa;
                pd_lsa = pd_lsa->next; 
                free(to_free_lsa);
            }
            else
            {
                pre_pd_lsa = pd_lsa;
                pd_lsa = pd_lsa->next;
            }
        }
    }

    return;
}

void remove_pd_ack(uint32_t snder_id, uint32_t seq_num, struct sockaddr_in addr)
{
    pend_lsa_t *pd_lsa;
    pend_lsa_t *pre_pd_lsa = NULL;

    // find the first match is the bucket
    HT_FIND(&g_pd_ack_ht, pend_lsa_t, snder_id, snder_id, pd_lsa);
    if(pd_lsa)
    {
        // find the exact one, need 8 byte less cmp for zero
        while(pd_lsa->seq_num != seq_num 
              || 0 != memcmp(&addr, &pd_lsa->addr, 
                             sizeof(struct sockaddr_in) - 8))
        {
            pre_pd_lsa = pd_lsa;
            pd_lsa = pd_lsa->next;
            if(!pd_lsa)
                break;
        }

        if(pd_lsa)
        {
            // in the first bucket
            if(pre_pd_lsa == NULL)
            {
                HT_REMOVE(&g_pd_ack_ht, pend_lsa_t, snder_id, snder_id);
            }
            else
            {
                pre_pd_lsa->next = pd_lsa->next;
            }
            // free pd lsa mem
            free(pd_lsa);
        }
    }
}

// positive reutrn value indicates need set wrset
int clnt_process_rt_inf(int fd)
{
    assert(fd < FD_SETSIZE);
    assert(clnt_set[fd].sock_type == SOCKET_TYPE_UDP);
   
    lsa_header_t lsa_h; 
    char *lsa_body = NULL;
    char *lsa_buff = NULL;
    size_t buff_sz;

    socklen_t sklen = sizeof(struct sockaddr_in);
    int need_wrset = 0;
    
    lsa_t *lsa;

    struct sockaddr_in addr;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_family = AF_INET;
 
    // need MSG_PEEK here, read the entire diagram next time
    if(sizeof(lsa_header_t) != recvfrom(fd, &lsa_h, sizeof(lsa_header_t), 
                                        MSG_DONTWAIT | MSG_PEEK,
                                        (struct sockaddr *)&addr, &sklen))
    {
        // discard the diagram
        recvfrom(fd, &lsa_h, sizeof(lsa_header_t), MSG_DONTWAIT | MSG_PEEK,
                                        (struct sockaddr *)&addr, &sklen);
        return need_wrset;
    }

    if(lsa_h.type == LSA_TYPE_ACK)
    {
        dbg_print(1, "recv ack from %s %d\n", inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));

        recvfrom(fd, &lsa_h, sizeof(lsa_header_t), MSG_DONTWAIT,
                                    (struct sockaddr *)&addr, &sklen);
        
        remove_pd_ack(lsa_h.snder_id, lsa_h.seq_num, addr);
        
    }  //type = ack ends
    else if(lsa_h.type == LSA_TYPE_AD)
    {
        dbg_print(1, "recv ad from %s %d\n", inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));

        lsa_buff = alloc_lsa_buff(&lsa_h, &buff_sz);
        if(!lsa_buff)
            return need_wrset;
        lsa_body = lsa_buff + sizeof(lsa_header_t);

        // read all data
        if(buff_sz != recvfrom(fd, lsa_buff, buff_sz, MSG_DONTWAIT,
             (struct sockaddr *)&addr, &sklen))
        {
            perror("recv wrong");
            free(lsa_buff);
            return need_wrset;
        }
        dbg_print_lsa(lsa_buff);

        HT_FIND(&g_lsa_ht, lsa_t, snder_id, lsa_h.snder_id, lsa);

        if(lsa_h.snder_id == g_nid)
        {        
            ; // ignore 
        }
        else if(lsa_h.seq_num == 0 && lsa != NULL && 0 < lsa->seq_num) //server restat, wrong seq number
        {
            // create type_seq annonce
            prep_seq_pkt(&lsa_h, lsa->seq_num, addr, fd);
            need_wrset = 1;
        } 
        else if(lsa == NULL || lsa_h.seq_num > lsa->seq_num) //valid seq number
        {
            node_info_t *nb;
            lsa_t *new_lsa;
            // update lsa entr
            new_lsa = update_lsa_entr(&lsa_h, lsa_body, buff_sz - sizeof(lsa_header_t));
            if(new_lsa)
            {
                pft_add_obj_frm_lsa(new_lsa);
            }

            
            HT_FIND(&g_nb_nodes_ht, node_info_t, nid, lsa_h.snder_id, nb);
            if(nb && nb->is_valid == NODE_INFO_INVALID)
            {
                dbg_print(3, "recv from nb, re-valid nv\n");
                valid_nb(nb);
                reload_local_lsa();
            }
            
            update_route_info();

            // need flood to other
            if(lsa_h.ttl - 1 > 0)
            {
                prep_flood_lsa(lsa_h.snder_id, addr, fd); 
            }
            
            //create ack packet
            prep_ack_pkt(&lsa_h, addr, fd);
            need_wrset = 1;

        } // ends valid seq number
        else if(lsa_h.seq_num <= lsa->seq_num) // less seq num
        {
            //create ack packet
            dbg_print(5, "less seq num\n");
            prep_ack_pkt(&lsa_h, addr, fd);
            need_wrset = 1;
        }

        free(lsa_buff);
    } //ad type ends
    else if(lsa_h.type == LSA_TYPE_SEQ)
    {
        dbg_print(1, "recv seq from %s %d\n", inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));
        recvfrom(fd, &lsa_h, sizeof(lsa_header_t), MSG_DONTWAIT,
                                    (struct sockaddr *)&addr, &sklen);
       
        //imply: sequence number is 0
        remove_pd_ack(lsa_h.snder_id, 0, addr);

        if(lsa_h.snder_id != g_nid) // not local
        {
            // try to update, maybe already updated by other seq type announce
            update_seq_num(lsa_h.snder_id, lsa_h.seq_num);
        }
        else  // my lsa
        {
            if(update_seq_num(lsa_h.snder_id, lsa_h.seq_num + 1))
            {
                clnt_prep_flood_lc_lsa(fd);
                reset_adv_time();
                need_wrset = 1;
            }
        }
    
    }
    return need_wrset;
}

void dbg_print_lsa(char *buff)
{
    lsa_header_t *lsa_h;
    obj_entr_t *obj_base;
    lnk_entr_t *lnk_base;

    lsa_h = (lsa_header_t *)buff;

    fprintf(stderr, "*********************recved lsa*********\n");
    fprintf(stderr, "ver %hhu  ", lsa_h->ver);
    fprintf(stderr, "ttl %hhu  ", lsa_h->ttl);
    fprintf(stderr, "type %hu  ", lsa_h->type); 
    fprintf(stderr, "snder id %u  ", lsa_h->snder_id);
    fprintf(stderr, "seq_num %u  ", lsa_h->seq_num); 
    fprintf(stderr, "num lnk %u  ", lsa_h->num_lnk_entr); 
    fprintf(stderr, "num obj %u\n", lsa_h->num_obj_entr); 

    lnk_base = (lnk_entr_t *)(buff + sizeof(lsa_header_t));
    obj_base = (obj_entr_t *)(buff + sizeof(lsa_header_t)
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


void prep_retran_pkt(int fd, pend_lsa_t *pd_lsa)
{
    assert(fd < FD_SETSIZE);
    assert(clnt_set[fd].sock_type == SOCKET_TYPE_UDP);

    //pd_lsa->time_left = retran_timeout_interval;
    lsa_update_retran_timeout(pd_lsa); 
    // insert to list to be send
    pd_lsa->next = clnt_set[fd].pd_lsa_head;
    clnt_set[fd].pd_lsa_head = pd_lsa;
}


static char *alloc_lsa_buff(lsa_header_t *head, size_t *size)
{
    *size = sizeof(lsa_header_t) + head->num_lnk_entr * sizeof(lnk_entr_t)
            + head->num_obj_entr * sizeof(obj_entr_t);

    return malloc(*size);
}

static int prep_ack_pkt(lsa_header_t *head, struct sockaddr_in addr, int fd)
{
    pend_lsa_t *pd_lsa;
    pd_lsa = lsa_create_ack_ann(head, addr);
    if(pd_lsa)
    {
        pd_lsa->next = clnt_set[fd].pd_lsa_head;
        clnt_set[fd].pd_lsa_head = pd_lsa;
    }

    return 0;
}

static int prep_seq_pkt(lsa_header_t *head, uint32_t seq_num, 
                        struct sockaddr_in addr, int fd)
{
    pend_lsa_t *pd_lsa;
    pd_lsa = lsa_create_seq_ann(head, seq_num, addr);
    if(pd_lsa)
    {
        pd_lsa->next = clnt_set[fd].pd_lsa_head;
        clnt_set[fd].pd_lsa_head = pd_lsa;
    }
    return 0; 
}

int clnt_prep_flood_lc_lsa(int rd_sock)
{
    struct sockaddr_in excpt_addr;
    memset(&excpt_addr, 0, sizeof(struct sockaddr_in));

    return prep_flood_lsa(g_nid, excpt_addr, rd_sock);
}

static int prep_flood_lsa(uint32_t snder_id, struct sockaddr_in excpt_addr, 
                          int rd_sock)
{
    struct sockaddr_in addr;
    size_t i;
    
    //char *p;
    lsa_t *llsa = NULL;
    node_info_t *ninf;
    pend_lsa_t *pd_lsa;

    HT_FIND(&g_lsa_ht, lsa_t, snder_id, snder_id, llsa);
    assert(llsa);

    for(i = 0; i < g_nb_nodes_ht.bucket_size; i++)
    {
        ninf = ((node_info_t **)g_nb_nodes_ht.table_base)[i];
        while(ninf)
        {
            if(!ninf->is_valid) // don't send to down neighbor
            {
                ninf = ninf->next;
                continue;
            }
            // don't send to myself, and the owen of the lsa
            if(ninf->nid != g_nid && ninf->nid != snder_id)
            {
                inet_pton(AF_INET, ninf->ip, &(addr.sin_addr)); 
                addr.sin_port = htons(ninf->rd_port);
                addr.sin_family = AF_INET;

                //don't send back to whom sent the packet to me
                if(!memcmp(&excpt_addr, &addr, sizeof(struct sockaddr) - 8))
                {
                        ninf = ninf->next;
                        continue;
                }

                pd_lsa = lsa_create_ad_ann(llsa, addr);
                // insert to list to be send
                if(pd_lsa)
                {
                    pd_lsa->next = clnt_set[rd_sock].pd_lsa_head;
                    clnt_set[rd_sock].pd_lsa_head = pd_lsa;
                }
                else
                {
                    goto err;
                }

            }
            ninf = ninf->next;
        } 
    }
    
    if(snder_id == g_nid)
    {
        (llsa->seq_num)++;
    }
    dbg_print(0, "seq after flood is %u\n", llsa->seq_num);
    return 0;

err:
    if(snder_id == g_nid)
    {
        (llsa->seq_num)++;
    }
 
    return -1;

}

