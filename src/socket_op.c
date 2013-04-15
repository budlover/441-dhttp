#include "socket_op.h"
#include <stdlib.h>


#define LISTEN_BACKLOG 50


int socket_nonblk(int type, int protocol)
{
    return socket(AF_INET, type | SOCK_NONBLOCK, 0); 
}

void init_addr_struct(struct sockaddr_in *addr, int port)
{
    addr->sin_family = AF_INET;
    addr->sin_port = htons(port);
    addr->sin_addr.s_addr = INADDR_ANY;
}

int bind_socket_addr(int sock, struct sockaddr_in *addr)
{
    return bind(sock, (struct sockaddr *)addr, sizeof(struct sockaddr_in));
}

int listen_sock(int sock)
{
    return listen(sock, LISTEN_BACKLOG);
    
}

/*
int recv_wrapper(int sock, SSL *ssl, SOCKET_TYPE type, buf_chain_t *buf_inf)
{
   if(type == SOCKET_TYPE_NONE_SSL)
   {
        return recv(sock, buf_inf->payload + buf_inf->len, 
                    BUFBLK_SIZE - buf_inf->len , 
                    MSG_DONTWAIT);
   } 
   else
   {
       return SSL_read(ssl, buf_inf->payload + buf_inf->len, 
                       BUFBLK_SIZE - buf_inf->len);
   }
}

int send_wrapper(int sock, SSL *ssl, SOCKET_TYPE type, const char *buf, int size)
{
    int tmp;
    if(type == SOCKET_TYPE_NONE_SSL)
    {
        return send(sock, buf, size, MSG_DONTWAIT);
    } 
    else
    {
        tmp = SSL_write(ssl, buf, size);
        ERR_print_errors_fp(stderr);
        tmp = SSL_get_error(ssl, tmp);
        return tmp;
    }
}
*/
