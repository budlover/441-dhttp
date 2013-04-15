#ifndef __SELECT_TIMEOUT_H__
#define __SELECT_TIMEOUT_H__

#include <stdint.h>
#include <sys/time.h>
#include "ht.h"
#include "lsa.h"
#include "client_info.h"

#define ONESEC 1000000                /* 1s */
#define DBG_TIME_OUT

void set_adv_timeout_interval(long interval);

void init_select_interval(struct timeval *tv, long check_interval);

void handle_time_out(int rd_sock, fd_set *wrset, long check_interval);

void reset_adv_time();
int is_timeout(struct timeval *tv);
#endif
