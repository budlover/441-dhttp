#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dijkstra.h"
#include "ht.h"
#include "lsa.h"
#include "config.h"

static uint32_t *g_node_id;
static int *g_graph;
route_table_t g_route_table;

extern hash_info_t g_lsa_ht;
extern hash_info_t g_nb_nodes_ht;
extern uint32_t g_nid;

void init_route_table()
{
    g_route_table.num_entry = 0;
    g_route_table.table = NULL;

    return;
}

static int get_index_by_nodeid(uint32_t node_id, int num_node)
{
    int index;
    
    for(index = 0; index < num_node; index++)
    {
        if (g_node_id[index] == node_id) return index;
    }

    return -1;
}

/* This two function are only for debugging */
#ifdef DBG_ROUTE_TABLE

static void print_shortest_distance(int num_node, int d[])
{
	int i;

	fprintf(stdout, "Distances:\n");
	for (i = 0; i < num_node; i++)
		fprintf(stdout, "%10u", g_node_id[i]);
	fprintf(stdout, "\n");
	for (i = 0; i < num_node; i++) {
		fprintf(stdout, "%10d", d[i]);
	}
	fprintf(stdout, "\n");
}


/*
 * Prints the shortest path from the source to dest.
 *
 * dijkstra(int) MUST be run at least once BEFORE
 * this is called
 */
static void print_path(int dest, int prev[])
{
	if (prev[dest] != PREV_HOP_NO)
		print_path(prev[dest], prev);
	fprintf(stdout, "%u ", g_node_id[dest]);
    return;
}
#endif

/* find out which is the next hop for a give destination */
static void get_next_hop(int src, int dst, int prev[], int *next_hop)
{
    if (prev[dst] == PREV_HOP_NO)
    {
        if (src != dst)
        {
            /* this is an unreachable node */
            *next_hop = NEXT_HOP_NO;
            return;
        }
        else
        {
            *next_hop = src;
            return;
        }
    }
    
	if (prev[dst] != src)
		get_next_hop(src, prev[dst], prev, next_hop);
    else
        *next_hop = dst;
    
	return;
}

/* most important part, dijkstra algo */
static void dijkstra(int source, int num_node, int graph[], int d[], int prev[])
{
	int i, k, mini;
	int visited[GRAPHSIZE];

	for (i = 0; i < num_node; i++)
    {
		d[i] = INFINITY;
		prev[i] = PREV_HOP_NO;  /* no path has yet been found to i */
		visited[i] = 0;         /* the i-th element has not yet been visited */
	}

	d[source] = 0;

	for (k = 0; k < num_node; k++)
    {
        /* visit a new node each round */
		mini = -1;
		for (i = 0; i < num_node; i++)
		{
			if (!visited[i] && ((mini == -1) || (d[i] < d[mini])))
				mini = i;
		}

		visited[mini] = 1;

        /* update the neighbor of new visited node */
        /* (dist + mini*node_num)[i] means line mini, colum i, update all 
           neighbors of mini */
		for (i = 0; i < num_node; i++)
		{
			if ((graph + mini*num_node)[i])
			{
				if (d[mini] + (graph + mini*num_node)[i] < d[i])
                {
					d[i] = d[mini] + (graph + mini*num_node)[i];
					prev[i] = mini;
				}
			}
		}
	}
}

/* this is for binary search */
static int cmp_dest_nid(const void *p1, const void *p2)
{
    uint32_t nid1 = ((const table_entry_t *) p1)->dest;
    uint32_t nid2 = ((const table_entry_t *) p2)->dest;

    if (nid1 < nid2) return -1; 
    else if (nid1 == nid2) return 0;
    else return 1;
}

/* to form a new routing table */
static void fill_route_table(int src, int num_node, int d[], int prev[])
{
    table_entry_t *tmp_table = NULL;
    int i;
    int next_hop = NEXT_HOP_NO;
    
    /* free the old routing table */
    if (g_route_table.num_entry != 0)
    {
        free(g_route_table.table);
        g_route_table.num_entry = 0;
        g_route_table.table = NULL;
    }

    /* create the new routing table */
    tmp_table = (table_entry_t *)malloc(num_node * sizeof(table_entry_t));
    for(i = 0; i < num_node; i++)
    {
        tmp_table[i].dest = g_node_id[i];
        
        get_next_hop(src, i, prev, &next_hop);
        tmp_table[i].next_hop = (next_hop != NEXT_HOP_NO) ? g_node_id[next_hop] : NEXT_HOP_NO; 
        
        tmp_table[i].cost = d[i];
    }

    /* sort the routing table by destination node id */
    qsort(tmp_table, num_node, sizeof(table_entry_t), cmp_dest_nid);

    g_route_table.table = tmp_table;
    g_route_table.num_entry = num_node;

    return;
}

#ifdef DBG_ROUTE_TABLE
static void print_route_table()
{
    int i;
    
    fprintf(stdout, "************* routing table *****************\n");
    for (i = 0; i < g_route_table.num_entry; i++)
    {
        fprintf(stdout, "dst: %d, nexthop: %d, cost: %d\n",
                g_route_table.table[i].dest,
                g_route_table.table[i].next_hop,
                g_route_table.table[i].cost);
    }
    fprintf(stdout, "*********************************************\n");
    
    return;
}
#endif

/* whether we already have this node in index array or not */
static int nodeid_already_in(int num_node, uint32_t node_id)
{
    int i;
    
    for(i = 0; i < num_node; i++)
    {
        if(g_node_id[i] == node_id) return SUCCESS;
    }

    return ERROR;
}

#ifdef DBG_ROUTE_TABLE
static void print_graph(int num_node)
{
    int i;
    int j;

    fprintf(stdout, "graph:\n");
    for (i = 0; i < num_node; i++)
    {
        for (j = 0; j < num_node; j++)
        {
            fprintf(stdout, "%d ", (g_graph + i*num_node)[j]);
        }

        fprintf(stdout, "\n");
    }

    return;
}
#endif

/* fill the 'g_graph' and the 'g_node_id' array */
static void construct_graph(int num_node, int num_self_nbr, uint32_t *self_nbr_id)
{
    int row, col;
    int i, j;
    uint32_t *tmp_node_id;
    lsa_t *lsa_node = NULL;
    lsa_t *p = NULL;
    int num_link = 0;
    lnk_entr_t *nbr_id = NULL;
    
    g_node_id = (uint32_t *)malloc(num_node * sizeof(int));
    memset(g_node_id, 0xFF, num_node * sizeof(int));
    tmp_node_id = g_node_id;
    
    g_graph = (int *)malloc(num_node*num_node * sizeof(int));
    memset(g_graph, 0, num_node*num_node * sizeof(int));

    /* index for self node is 0 */
    g_node_id[SELF_NODE_INDEX] = g_nid;
    tmp_node_id++;
    
    /* fill the 'g_node_id' array, all other nodes */
    for (i = 0; i < g_lsa_ht.bucket_size; i++)
    {   
        lsa_node = (lsa_t *)(g_lsa_ht.table_base[i]);
        for(; lsa_node != NULL; lsa_node = lsa_node->next)
        {
            /* do not add your self again */
            if (g_nid == lsa_node->snder_id) continue;

            *tmp_node_id = lsa_node->snder_id;
            tmp_node_id++;
        }
    }
    
#ifdef DBG_ROUTE_TABLE
    for (i = 0; i < num_node; i++)
    {
        fprintf(stdout, "g_node_id[%d]: %u\n", i, g_node_id[i]);
    }
#endif

    /* fill the 'g_graph' */
    /* fill the first row (row 0) for yourself */
    for (j = 0; j < num_self_nbr; j++)
    {
        col = get_index_by_nodeid(self_nbr_id[j], num_node);
        if (-1 != col) (g_graph + 0*num_node)[col] = LINK_COST;
    }

    /* fill other rows for other nodes */
    for (i = 1; i < num_node; i++)
    {
        HT_FIND(&g_lsa_ht, lsa_t, snder_id, g_node_id[i], p);

        /* if LAS invalid, we do not consider this node can reach any other 
           nodes*/
        if(LSA_VALID != p->is_valid) continue;
        
        num_link = p->num_lnk_entr;
        nbr_id = (lnk_entr_t *)(p + 1);
        for (j = 0; j < num_link; j++)
        {
            row = i;
            col = get_index_by_nodeid(nbr_id->nid, num_node);

            /* we only add links to reachable nodes, if a node is not in 
               g_node_id, we do not add link to it */
            if (SUCCESS == nodeid_already_in(num_node, nbr_id->nid))
            {
                (g_graph + row*num_node)[col] = LINK_COST;
                //(g_graph + col*num_node)[row] = LINK_COST;            
                nbr_id += 1;
            }
        }    
    }

#ifdef DBG_ROUTE_TABLE   
    print_graph(num_node);
#endif

    return;
}

/* when we have the topology graph, call dijkstra */
static void update_route_table(int num_node)
{
    int *d = NULL;    /* shortest distance to every node */
    int *prev = NULL; /* previous node on the shortest path */
    int src = SELF_NODE_INDEX;      /* make the index of self node 0 */
#ifdef DBG_ROUTE_TABLE   
    int i;
    int next_hop = NEXT_HOP_NO;
#endif

    /* init all arrays */
    d = (int *)malloc(num_node * sizeof(int));
    prev = (int *)malloc(num_node * sizeof(int));

    dijkstra(src, num_node, g_graph, d, prev);

    /* uodate the routing table */
    fill_route_table(src, num_node, d, prev);

#ifdef DBG_ROUTE_TABLE     
	print_shortest_distance(num_node, d);
	fprintf(stdout, "\n");

	for (i = 0; i < num_node; i++)
    {	
        get_next_hop(src, i, prev, &next_hop);
        if (next_hop != NEXT_HOP_NO)
        {
            fprintf(stdout, "to %u, next hop is %u\n", g_node_id[i], g_node_id[next_hop]);
            fprintf(stdout, "Path to %u: ", g_node_id[i]);
    		print_path(i, prev);
            fprintf(stdout, "\n");
        }
        else
        {
            fprintf(stdout, "%u is an isolate node\n", g_node_id[i]);
        }
		fprintf(stdout, "\n");
	}

    print_route_table();
#endif

    /* free the temporary arrays */
    free(prev);
    free(d);
    
	return;
}

void get_nbr_nid(uint32_t *nbrs_id, int *num_nbr)
{
    int i;
    node_info_t *nbr_info;
    uint32_t *tmp = nbrs_id;
    int nbr_count = 0;
    
    /* find all nbr ids */
    for (i = 0; i < g_nb_nodes_ht.bucket_size; i++)
    {   
        nbr_info = (node_info_t *)(g_nb_nodes_ht.table_base[i]);
        for(; nbr_info != NULL; nbr_info = nbr_info->next) 
        {
            /* only valid nodes count nbr */
            if (NODE_INFO_VALID != nbr_info->is_valid) continue;
            
            *tmp = nbr_info->nid;
            tmp++;
            nbr_count++;
        }
    }

    *num_nbr = nbr_count;
    return;
}

/* entry func for this file */
void update_route_info()
{
    int num_node = g_lsa_ht.entr_cnt;
    uint32_t *nbrs_id = NULL;
    int num_nbr = 0;

    nbrs_id = malloc(g_nb_nodes_ht.entr_cnt * sizeof(int));
    get_nbr_nid(nbrs_id, &num_nbr);

#ifdef DBG_ROUTE_TABLE
    fprintf(stdout, "valid nbr cnt: %d\n", num_nbr);
    fprintf(stdout, "total node number: %d\n", num_node);
#endif
    construct_graph(num_node, num_nbr, nbrs_id);
    update_route_table(num_node);

    free(nbrs_id);
    free(g_node_id);
    free(g_graph);

    return;
}

/* return the index of entry in routing table */
table_entry_t *find_route_entry(uint32_t target_id)
{
    table_entry_t *table = g_route_table.table;
    int low = 0;
    int high = g_route_table.num_entry - 1;
    int middle;
    
    while (low <= high)
    {
        middle = low + (high - low)/2;
        if (target_id < (table[middle]).dest)
            high = middle - 1;
        else if (target_id > (table[middle]).dest)
            low = middle + 1;
        else
            return &(table[middle]);
    }
    
    return NULL;
}
