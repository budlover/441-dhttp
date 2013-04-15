#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>

#include "ht.h"
#include "str_helper.h"
#include "config.h"
#include "socket_op.h"
#include "client_info.h"
#include "rt_ctrl.h"
#include "lsa.h"
#include "sel_timeout.h"
#include "pft.h"
#include "dijkstra.h"

#define ARGC 8

#define ARG_IDX_NODE_ID 1
#define ARG_IDX_CONFIG_FILE 2
#define ARG_IDX_FILE_LIST 3
#define ARG_IDX_ADV_CYCLE 4
#define ARG_IDX_NEIGHBOR_TIMEOUT 5
#define ARG_IDX_RETRAN_TIMEOUT 6
#define ARG_IDX_LSA_TIMEOUOT 7


extern hash_info_t g_lfile_ht;
extern hash_info_t g_nb_nodes_ht;
extern uint32_t g_nid;
extern int g_lc_port;    //local port
extern int g_server_port;
extern int g_rd_port;    //routing daemon port

void usage();

int main(int argc, char *argv[])
{
    int lc_sock = 0;
    struct sockaddr_in addr;
    fd_set rdset;
    fd_set wrset;
    fd_set tmp_rdset;
    fd_set tmp_wrset;

    long ad_time;
    long nb_timeout;
    long retran_timeout;
    long lsa_timeout;
    long check_interval;
    int tmp;
    struct timeval tv;

    int rd_sock = 0;

    if(argc != ARGC)
    {
        goto err;
    }

    if(0 != str_to_uint32(argv[ARG_IDX_NODE_ID], &g_nid))
    {
        goto err;        
    }

    if(0 != load_neighbor_nodes(argv[ARG_IDX_CONFIG_FILE]))
    {
        fprintf(stderr, "node file not found or wrong record format\n");
        goto err;
    }

    if(0 != load_localfile(argv[ARG_IDX_FILE_LIST]))
    {
        fprintf(stderr, "local file not found or wrong local file format\n");
        goto err;
    }
    
    if(0 != str_to_num(argv[ARG_IDX_ADV_CYCLE], &tmp))
    {
        goto err;
    }
    ad_time = ONESEC * (long)tmp;
    set_adv_timeout_interval(ad_time);

    if(0 != str_to_num(argv[ARG_IDX_NEIGHBOR_TIMEOUT], &tmp))
    {
        goto err;
    }
    nb_timeout = ONESEC * (long)tmp;
    set_nb_timeout_interval(nb_timeout);

    if(0 != str_to_num(argv[ARG_IDX_RETRAN_TIMEOUT], &tmp))
    {
        goto err;
    }
    retran_timeout = ONESEC * (long)tmp;
    set_retran_timeout_interval(retran_timeout);

    if(0 != str_to_num(argv[ARG_IDX_LSA_TIMEOUOT], &tmp))
    {
        goto err;
    }
    lsa_timeout = ONESEC * (long)tmp;
    set_lsa_timeout_interval(lsa_timeout);

    if(retran_timeout < 2)
    {
        fprintf(stderr, "retran timeout should be larger than 2 seconds\n");
    }
    check_interval = ONESEC * 2;

    dbg_print_lfile();
    dbg_print_nb_nodes();

    node_info_t *p;
    HT_FIND(&g_nb_nodes_ht, node_info_t, nid, g_nid, p);
    if(!p)
       goto err; 
    g_lc_port = p->local_port;
    g_rd_port = p->rd_port;
    g_server_port = p->server_port;

    // init link state announce
    lsa_t *llsa;
    assert(NULL != (llsa = init_lsa()));
    
    // init obj prefix tree
    pft_add_obj_frm_lsa(llsa);

    // set the init routing table
    update_route_info();

    //create the local TCP sock
    if(-1 == (lc_sock = socket_nonblk(SOCK_STREAM, 0)))
    {
        fprintf(stderr, "Error: Can't create socket\n"); 
        goto err;
    }
    init_addr_struct(&addr, g_lc_port);
    if(0 != bind_socket_addr(lc_sock, &addr))
    {
        perror("Err bind local port");
        //print err
        goto err;
    }
    if(0 != listen_sock(lc_sock))
    {
        perror("Err, Can't listen on local port");
        goto err;
    }
    assert(!create_clnt_info(lc_sock, SOCKET_TYPE_TCP));

    // create rd UDP sock
    if(-1 == (rd_sock = socket_nonblk(SOCK_DGRAM, IPPROTO_UDP)))
    {
        fprintf(stderr, "Error: Can't create socket\n"); 
        goto err;
    }
    init_addr_struct(&addr, g_rd_port);
    if(0 != bind_socket_addr(rd_sock, &addr))
    {
        perror("err bind rd port");
        goto err;
    }
    assert(!create_clnt_info(rd_sock, SOCKET_TYPE_UDP));

    // init pending ack table
    init_pd_ack_table();
    
    FD_ZERO(&wrset);

    // set read local sock
    FD_ZERO(&rdset);
    FD_SET(lc_sock, &rdset);
    
    // set read routing sock
    FD_SET(rd_sock, &rdset);

    //prepare initial flood local lsa
    clnt_prep_flood_lc_lsa(rd_sock);
    FD_SET(rd_sock, &wrset);

    init_select_interval(&tv, check_interval);
    while(1)
    {
        int ret;
        tmp_rdset = rdset;
        tmp_wrset = wrset;
        ret = select(FD_SETSIZE, &tmp_rdset, &tmp_wrset, NULL, &tv);
        if(ret > 0)
        {
            process_read(&rdset, &tmp_rdset, &wrset, lc_sock, rd_sock);
            process_write(&wrset, &tmp_wrset, &rdset, lc_sock, rd_sock);
        }

        if(ret >= 0)
        {
            if(is_timeout(&tv))
            {
                handle_time_out(rd_sock, &wrset, check_interval);
                // reset interval time
                init_select_interval(&tv, check_interval);
            }
        }
        else
        {
            assert(0);
        }
    }
    

    return 0;

err:
    // no need to free memory here, system help free when exit
    // but better to cloes socket
    if(lc_sock)
        close(lc_sock);

    if(rd_sock)
        close(rd_sock);

    usage();
    exit(0);
}

void usage()
{
    fprintf(stderr, "usage: ./routed <nodeid> <config file> <file list> "
                    "<adv cycle time> <neighbor timeoout> <retran timeout> "
                    "<LSA timeout>\n");
}
