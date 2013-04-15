#ifndef _CLIENT_INFO_H_
#define _CLIENT_INFO_H_

#include <sys/types.h>
#include <stdint.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

#include "buf_ctrl.h"
#include "parser.h"
#include "lsa.h"
#include "pft.h"


#define SEND_BUFF_LEN 3072 


typedef enum _SOCKET_TYPE
{
    SOCKET_TYPE_TCP,
    SOCKET_TYPE_UDP
} SOCKET_TYPE;

typedef struct client_info
{
    SOCKET_TYPE sock_type;
    int is_exist;

    //recv buffer
    buf_chain_t *curr_buf;  // the current buf node for receiving
    buf_chain_t *first_buf; 

    pend_lsa_t *pd_lsa_head; // routing UDP packet to send

    //send buffer
    char *send_buf;
    size_t send_len;

    parser_info_t parser_inf;
    req_info_t req_inf;
} client_info_t;



void init_pd_ack_table();

int create_clnt_info(int fd, SOCKET_TYPE type);
void update_clnt_info(int fd);
void remove_clnt_info(int fd);


int get_next_fd(fd_set *fdset);
int recv_clnt_data(int fd);
int get_clnt_full_req(int fd);
int need_close_clnt(int fd);

int process_clnt_request(int fd, int *need_flood, int rd_sock);
int send_clnt_resp(int fd);
client_info_t *get_clnt_info(int fd);

void clnt_rm_pd_ad_ann_by_addr(struct sockaddr_in addr);

void prep_retran_pkt(int fd, pend_lsa_t *pd_lsa);

// routing infor
int clnt_send_rd_info(int fd);
int clnt_process_rt_inf(int fd);
int clnt_prep_flood_lc_lsa(int fd);
#endif
