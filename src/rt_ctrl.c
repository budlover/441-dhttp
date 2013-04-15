#define DEBUG
#include <stdio.h>
#include <netinet/in.h>
#include <assert.h>
#include <sys/types.h>
#include <errno.h>
#include <sys/socket.h>

#include "rt_ctrl.h"
#include "client_info.h"
#include "buf_ctrl.h"
#include "parser.h"
#include "dbg_helper.h"

/* accept new connection request */
static int accept_new(int lc_sock)
{
    int new_sock;
    struct sockaddr_in addr;
    socklen_t addr_sz = sizeof(struct sockaddr_in);

    new_sock = accept(lc_sock, (struct sockaddr *)&addr, &addr_sz);

    if(-1 == new_sock)
    {
        perror("new sock failed");
        return -1; 
    }

    if(-1 == create_clnt_info(new_sock, SOCKET_TYPE_TCP))
    {
        close(new_sock);
        return -1;
    }

    return new_sock;
}

/*  
    Process sock read. Two type of sock could be read, TCP or UDP
    TCP sock is local port. In this function, it only read the sock, and store 
    data into the buffer belonged to the socket.
    UDP is routing port. It process one LSA each time. Prepare the response
    and update routing info accordingly.
 */
void process_read(fd_set *rdset, fd_set *tmp_rdset, fd_set *wrset,
                  int lc_sock, int rd_sock)
{
    int sock;
    int new_sock;

    rd_sock = rd_sock;

    while(-1 != (sock = get_next_fd(tmp_rdset)))
    {
        dbg_print(0, "to read sock %d\n", sock);
        client_info_t *clnt_inf = get_clnt_info(sock);

        // local request
        if(clnt_inf->sock_type == SOCKET_TYPE_TCP)
        {
            if(sock == lc_sock)   // listen sock
            {
                new_sock = accept_new(lc_sock);
                if(-1 != new_sock)
                {
                    dbg_print(2, "new sock %d\n", new_sock);
                    FD_SET(new_sock, rdset);
                }
                continue;
            }
            else
            {
                int rev;
                
                rev = recv_clnt_data(sock);
                if(rev > 0)
                {
                    FD_SET(sock, wrset);
                }
                else if(0 == rev) //peer close socket
                {
                    close(sock);
                    FD_CLR(sock, wrset);
                    FD_CLR(sock, rdset);
                    remove_clnt_info(sock);
                    dbg_print(2, "peer close socket %d\n", sock);
                }
                else if(-1 ==rev && EWOULDBLOCK != errno)
                {
                    fprintf(stderr, "??? what happens when read?\n");
                    perror("?");
                    assert(0);
                }
            }
        }//end tcp
        else // routing port, process routing LSA
        {
            if(clnt_process_rt_inf(sock) > 0)
            {
                FD_SET(sock, wrset);
            }
        }
    }
}


/* 
   Two type of socket could be processed.
   TCP sock is local port. It parse the request and process it
   accordingly, either GETRD or ADDFILE.
   UDP is routing port. It send routing announcement to other one.
 */
void process_write(fd_set *wrset, fd_set *tmp_wrset, fd_set *rdset,
                  int lc_sock, int rd_sock)
{
    int sock;

    while(-1 != (sock = get_next_fd(tmp_wrset)))
    {
        client_info_t *clnt_inf = get_clnt_info(sock);
        dbg_print(0, "to write sock %d\n", sock);
        int ret;
        int need_flood = 0;
    
        // process local request    
        if(clnt_inf->sock_type == SOCKET_TYPE_TCP) 
        {
            ret = get_clnt_full_req(sock);
            if(ret) //new full request
            {
                process_clnt_request(sock, &need_flood, rd_sock);
                if(need_flood)
                {
                    FD_SET(rd_sock, wrset);
                }
                send_clnt_resp(sock);

                if(need_close_clnt(sock))
                {
                    dbg_print(5, "bad request, close sock %d\n", sock);
                    FD_CLR(sock, wrset);
                    FD_CLR(sock, rdset);

                    close(sock);
                    remove_clnt_info(sock);
                }
                else
                {
                    update_clnt_info(sock);
                }
            }
            else //no full request 
            {
                FD_CLR(sock, wrset);
            }
        } //ends type = TCP
        else // type == UDP
        {
            clnt_send_rd_info(sock);
            FD_CLR(sock, wrset);
        }

    }//end while 
}
