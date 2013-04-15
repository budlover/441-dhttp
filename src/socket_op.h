#ifndef _SOCKET_OP_H_
#define _SOCKET_OP_H_
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>

#include "buf_ctrl.h"

int socket_nonblk(int type, int protocol);
void init_addr_struct(struct sockaddr_in *addr, int port);
int bind_socket_addr(int sock, struct sockaddr_in *addr);
int listen_sock(int sock);
#endif
