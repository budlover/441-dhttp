#ifndef _RESP_H_
#define _RESP_H_

#include "parser.h"
int fill_local_resp(char *buff, size_t len, const char *file_name, 
                    const char *ip, int server_port,
                    int remote_lc_port, const char *local_file_path,
                    REQUEST_METHOD method, REQUEST_STATUS status);

#endif
