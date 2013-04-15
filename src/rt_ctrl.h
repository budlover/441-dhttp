#ifndef _RT_CTRL_H_
#define _RT_CTRL_H_

#include <sys/select.h>
#include <sys/types.h>
#include <unistd.h>

void process_read(fd_set *rdset, fd_set *tmp_rdset, fd_set *wrset,
                  int lc_sock, int rd_sock);
void process_write(fd_set *wrset, fd_set *tmp_wrset, fd_set *rdset,
                  int lc_sock, int rd_sock);


#endif
