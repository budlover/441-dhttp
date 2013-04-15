/*
   The function for forwarding (obj lookup)
  */
#include <string.h>
#include <limits.h>

#include "forward.h"
#include "config.h"
#include "ht.h"
#include "dijkstra.h"
#include "pft.h"

extern hash_info_t g_lfile_ht;
extern hash_info_t g_nb_nodes_ht;

extern uint32_t g_nid;
extern int g_lc_port;    //local port
extern int g_rd_port;    //routing daemon port
extern int g_server_port;

/*
    Lookup local object, and fill in the result

    filename: the obj name to look for
    ip: the buffer to write the ip address of localhost
    server_port: the buffer to write the server port
    local_file_path: the buffer to write the local file relative path for the
                     looked for object
  */
static int lookup_lc_file(const char *filename, char *ip, int *server_port,
                          char *local_file_path)
{
    file_entry_t *p;
    HT_FIND(&g_lfile_ht, file_entry_t, file_name, filename, p);
    if(!p)
        return -1; //not found.
    
    strcpy(ip, "localhost");
    *server_port = g_server_port;
    strcpy(local_file_path, p->file_path);
    return 0;
}

/*
    Lookup an object. The longest prefix match will be applied on the lookup.

    filename: the obj name to look for
    ip: the buffer to write the ip address of localhost
    server_port: the buffer to write the server port
    local_file_path: the buffer to write the local file relative path for the
                     looked for object. If the object is stored locally and is
                     mathed as longed prefix
  */
int lookup_file(const char *filename, char *ip, int *server_port,
                int *remote_lc_port, char *local_file_path)
{
 
    node_info_t *ninf;  
    table_entry_t *rt_entr; 
   
    size_t i; 
    int min_cost = INT_MAX;
    uint32_t next_hop = UINT_MAX;

    darr_t *darr;           // the array contains the all nodes that
                            // has the obj

    darr_t *all_pre = NULL; // all prefix for the obj in the prefix tree
    darr_t *last_pre = NULL; // The longest prefix, except for exact match
    darr = pft_get_obj_list(filename, &last_pre, &all_pre);
    if(!darr)
        return -1;
    assert(all_pre);

    
    for(i = 0; i < darr->curr_elem; ++i)
    {
        if(-1 != get_elem_idx(all_pre, darr->arr[i]))
        {
            // skip prefix, for longest match
            continue;
        }

        rt_entr = find_route_entry(darr->arr[i]);
        if(!rt_entr)
            continue;
   
        // get the minimum cost     
        if(rt_entr->cost < min_cost)
        {
            min_cost = rt_entr->cost;
            next_hop = rt_entr->next_hop;
        }  
    } 

    if(next_hop == UINT_MAX && last_pre) // no exact match, may be prefix match
    {
        for(i = 0; i < last_pre->curr_elem; ++i)
        {
            rt_entr = find_route_entry(last_pre->arr[i]);
            if(!rt_entr)
                continue;
   
            // get the minimum cost     
            if(rt_entr->cost < min_cost)
            {
                min_cost = rt_entr->cost;
                next_hop = rt_entr->next_hop;
            } 
        }
    }

    if(next_hop == UINT_MAX)
        return -1;
   
    // the next hop must be a neighbor 
    HT_FIND(&g_nb_nodes_ht, node_info_t, nid, next_hop, ninf);
    if(!ninf)
        return -1;

    if(ninf->nid == g_nid) // local file
    {
        if(-1 == lookup_lc_file(filename, ip, server_port, local_file_path))
            return -1;

        *remote_lc_port = -1;
    }
    else
    {
        strcpy(ip, ninf->ip);
        *server_port = ninf->server_port;
        *remote_lc_port = ninf->local_port;
    }

    return 0;
}

