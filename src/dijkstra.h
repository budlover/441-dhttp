#ifndef __DIJKSTRA_H__
#define __DIJKSTRA_H__

#include <stdint.h>

#define GRAPHSIZE 2048
#define INFINITY GRAPHSIZE*GRAPHSIZE
#define MAX(a, b) ((a > b) ? (a) : (b))

#define NEXT_HOP_NO -1
#define PREV_HOP_NO NEXT_HOP_NO

#define SUCCESS 0
#define ERROR -1

#define LINK_COST 1

#define SELF_NODE_INDEX 0

typedef struct table_entry {
    uint32_t dest;
    uint32_t next_hop;
    int cost;
} table_entry_t;

typedef struct route_table {
    int num_entry;
    table_entry_t *table;    
} route_table_t;

/* debug swith here */
//#define DBG_ROUTE_TABLE

void init_route_table();
void update_route_info();
table_entry_t *find_route_entry(uint32_t target_id);

#endif
