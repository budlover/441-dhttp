/*
   The functions that operate on the prefix tree.
   The prefix will be leveled according to '/'.
   For example, a string "/a/b/cd/e" will be divided to 5 levels.
   "/", "a/", "b/", "cd/", and "e".
 */

#include "pft.h"
#include "config.h"
#include "lsa.h"
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

pf_node_t *g_obj_pft = NULL;

/*
    String split function. delim is the deliminator char to split each string.
    Each calling to this function will fill the buff with all chars from the
    start index(start_idx) to the first delim included. If no delim is found, 
    entire string from start_idx will be filled in to buff.
    next_field will be set to indicate the start index of the next field.
 */
static int get_next_field(const char *str, int start_idx, char delim, char *buff, int *next_field)
{
    int cnt = 0;
    const char *p = str + start_idx;

    while(*p)
    {
        cnt++;
        if(*p == delim)
        {
            //if(cnt != 0) // skip first delim
            p++;
            break;
        }
        p++;
    }
    if(cnt == 0)
        return -1;

    memcpy(buff, str + start_idx, cnt);
    buff[cnt] = '\0';

    *next_field = p - str;

    return 0;
}

/*
   create a new node
 */
static pf_node_t *new_node()
{
    pf_node_t *p;
    p = malloc(sizeof(pf_node_t));
    if(!p)
        return NULL;
    
    p->darr = NULL;
    p->next_peer = NULL;
    p->next_child = NULL;
    return p;
}

/*
    Add a new obj and the node id to the prefix tree.
    Repeate obj and nid association will not be added twice
 */
int pft_add_obj(char *obj, uint32_t nid)
{
    assert(strlen(obj) <= OBJ_NAME_LEN);
    char buff[OBJ_NAME_LEN + 1];
    int start_idx = 0;

    pf_node_t **curr_lv_rt = &g_obj_pft; // current level root prefix
    pf_node_t *curr_node = *curr_lv_rt;
    pf_node_t *target = NULL;

    while(0 == get_next_field(obj, start_idx, '/', buff, &start_idx))
    {
        // search the current level prefix
        while(curr_node)
        {
            if(!strcmp(buff, curr_node->prefix))
                break;

            curr_node = curr_node->next_peer;
        }
        
        if(!curr_node) // not found
        {
            curr_node = new_node();
            if(!curr_node)
                return -1;

            strcpy(curr_node->prefix, buff);
            curr_node->next_peer = *curr_lv_rt;
            *curr_lv_rt = curr_node;
        }
      
        target = curr_node; 
        curr_lv_rt = &curr_node->next_child;
        curr_node = *curr_lv_rt;
    }

    assert(target != NULL);
   
    // store the nid to the current prefix level node
    if(target->darr == NULL)
    {
        target->darr = new_darr();
    
        if(target->darr == NULL)
            return -1;
    }

    // check repeating before add
    if(-1 == get_elem_idx(target->darr, nid))
    {
        if(-1 == add_elem(target->darr, nid))
            return -1;
    }

    return 0;
}

/*
    Search the prefix tree for an obj, return the array of nodes that contains
    the object, if there is.
    The all_pre param will always be set with a not-NULL value. This array may
    contain all all nodes that claimed an aggregate prefix.
    The last_pre param may be set to point to a array containing nodes with
    the longest aggragate prefix match.

    For example, for a obj '/a/b/obj', if node 1, 2, 3 all claim to have the
    obj and node 1 also claim to have /a/, node 2 claims to have /a/b/.
    Then the darr return will indicate node 1, 2, 3,
    The last_pre will indicate node 2
    All pre will indicate node 1 and 2.
 */
darr_t *pft_get_obj_list(const char *obj, darr_t **last_pre, darr_t **all_pre)
{
    char buff[OBJ_NAME_LEN + 1];
    int start_idx = 0;

    pf_node_t *curr_node = g_obj_pft; 
    pf_node_t *target = NULL;

    static darr_t *al_pre = NULL;
    if(al_pre == NULL)
    {
        al_pre = new_darr(); 
        if(!al_pre)
            return NULL;
    }
    *all_pre = NULL;
    *last_pre = NULL;

    al_pre->curr_elem = 0;

    while(0 == get_next_field(obj, start_idx, '/', buff, &start_idx))
    {
        while(curr_node)
        {
            if(!strcmp(buff, curr_node->prefix))
                break;

            curr_node = curr_node->next_peer;
        }
        
        if(!curr_node) // not found
        {
            break;  // longest prefix match up to now
        }
      
        target = curr_node; 
        curr_node = curr_node->next_child;

        if(target->darr && target->darr->curr_elem != 0)
        {
            merge_darr(al_pre, target->darr);
            *last_pre  = target->darr;
        }
    }

    if(!target)
        return NULL;

    if(al_pre->curr_elem > 0) //remove the match node's last appned id
    {
        al_pre->curr_elem -= target->darr->curr_elem;
    }
    *all_pre = al_pre;

    return target->darr;
}

/* 
   Remove an object associated with a node
 */
void pft_rm_obj(char *obj, uint32_t nid)
{
    char buff[OBJ_NAME_LEN + 1];
    int start_idx = 0;
    size_t idx;

    pf_node_t *curr_node = g_obj_pft; 
    pf_node_t *target = NULL;

    while(0 == get_next_field(obj, start_idx, '/', buff, &start_idx))
    {
        while(curr_node)
        {
            if(!strcmp(buff, curr_node->prefix))
                break;

            curr_node = curr_node->next_peer;
        }
        
        if(!curr_node) // not found
        {
            return ;
        }
      
        target = curr_node; 
        curr_node = curr_node->next_child;
    }

    if(!target)
        return ;

    // check existance before remove
    if(-1 != (idx = get_elem_idx(target->darr, nid)))
    {
        rm_elem_by_idx(target->darr, idx);
    }
}


/* for test only */

void dbg_print_pft(pf_node_t *rt, int lev)
{
    if(!rt)
        return ;
   
    size_t i; 
    while(rt)
    {
        fprintf(stderr, "*************************************\n");
        fprintf(stderr, "level %d, prefix %s\n", lev, rt->prefix);
       
        if(rt->darr)
        {
            for(i = 0; i < rt->darr->curr_elem; ++i)
            {
                fprintf(stderr, "%d ", rt->darr->arr[i]);
            }
            fprintf(stderr, "\n");
        } 
        
        dbg_print_pft(rt->next_child, lev+1);
        rt = rt->next_peer;
    }
}

void dbg_print_pft_rt()
{
    fprintf(stderr, "*****************the pft *************************\n");
    dbg_print_pft(g_obj_pft, 0);
    fprintf(stderr, "*****************the pft ends*************************\n");
}

/*
   Add obj and node association from a lsa entry.
   All obj in the lsa will be added to the prefix tree.
 */
void pft_add_obj_frm_lsa(lsa_t *lsa)
{
    obj_entr_t *obj_base;
    obj_base = (obj_entr_t *)((char *)lsa + sizeof(lsa_t)
                + lsa->num_lnk_entr * sizeof(lnk_entr_t));

    uint32_t i;
    for(i = 0; i < lsa->num_obj_entr; ++i)
    {
        pft_add_obj(obj_base[i].obj, lsa->snder_id);
    }
    dbg_print_pft_rt();
}

/*
   Remove obj and node association from a lsa entry
   All obj in the lsa will be removed from the prefix tree.
 */
void pft_rm_obj_frm_lsa(lsa_t *lsa)
{
    obj_entr_t *obj_base;
    obj_base = (obj_entr_t *)((char *)lsa + sizeof(lsa_t) + lsa->num_lnk_entr * sizeof(lnk_entr_t));

    uint32_t i;
    for(i = 0; i < lsa->num_obj_entr; ++i)
    {
        pft_rm_obj(obj_base[i].obj, lsa->snder_id);
    }
    dbg_print_pft_rt();
}

