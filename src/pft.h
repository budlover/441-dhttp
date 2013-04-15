#ifndef _PFT_H_
#define _PFT_H_

#include "config.h"
#include "darr.h"
#include "lsa.h"

typedef struct pf_node
{
    char prefix[OBJ_NAME_LEN + 1];
    darr_t *darr;    
    struct pf_node *next_peer;
    struct pf_node *next_child;
} pf_node_t;

//darr_t *pft_get_obj_list(const char *obj);

darr_t *pft_get_obj_list(const char *obj, darr_t **last_pre, darr_t **all_pre);

void dbg_print_pft_rt();
void pft_add_obj_frm_lsa(lsa_t *lsa);
void pft_rm_obj_frm_lsa(lsa_t *lsa);
#endif
