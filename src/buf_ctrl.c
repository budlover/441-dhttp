#define DEBUG
#include "dbg_helper.h"
#include "buf_ctrl.h"
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <stdlib.h>

#ifdef DBG_LEVEL
    #undef DBG_LEVEL
#endif
#define DBG_LEVEL 1


// get a byte from the buffer, update the location infomation, if the
// function succeed
static int get_next_byte(buf_chain_t **curr_buf_node, int *next_byte_idx, char *c)
{
    buf_chain_t *buf_node = *curr_buf_node; 
    char *data = buf_node->payload;

    if(*next_byte_idx > buf_node->len)
        return NXT_BYTE_NO_MORE; 

    *c = data[*next_byte_idx];
    (*next_byte_idx)++;
    if(*next_byte_idx == BUFBLK_SIZE)
    {
        *curr_buf_node = buf_node->next;
        *next_byte_idx = 0;
    }
    return 0;
}

// get several bytes as specified, the bytes may cross buffer boundary
// if succeed, the location info will be updated.
int get_bytes(buf_chain_t **curr_buf_node, int *next_byte_idx, char *buf, int size)
{
    buf_chain_t *tmp_node = *curr_buf_node; 
    int tmp_nxt_idx = *next_byte_idx;
    char c;

    int i;
    for(i = 0; i < size; i++)
    {
        if(0 == get_next_byte(&tmp_node, &tmp_nxt_idx, &c))
            buf[i] = c;
        else
            break;
    }

    if(i == size)
    {
        *curr_buf_node = tmp_node;
        *next_byte_idx = tmp_nxt_idx;

        return 0;
    }
    else
    {
        return GET_BYTES_FAILED;
    }
}

// get nexst field deliminated by ' '(a space)
// filling the output buffer. This may cross the buffer boundary.
// If the fucntion succeed, the location info will be updated
int get_next_field(buf_chain_t **curr_buf_node, int *next_byte_idx, char *buf, int buf_size)
{
    assert(*curr_buf_node);
    buf_chain_t *tmp_node = *curr_buf_node; 
    int tmp_nxt_idx = *next_byte_idx;
    int cnt = 0; 
    int sp_found = 0;
    char c;

    while(0 == get_next_byte(&tmp_node, &tmp_nxt_idx, &c))
    {
        if(c != ' ')
            break;
    }

    buf[0] = c;
    cnt++;


    while(0 == get_next_byte(&tmp_node, &tmp_nxt_idx, &c))
    {
        cnt++;
        if(cnt > buf_size) //space will be replaced by '\0'
            return NXT_FIELD_FAILED;

        buf[cnt - 1] = c;

        if(c == ' ')
        {
            sp_found = 1;
            break;
        }
     }

    if(sp_found == 1)
    {
        buf[cnt - 1] = '\0'; //repalce the space
        dbg_print(1, "Function:%s:%.*s\n", __FUNCTION__, cnt, buf);
        *curr_buf_node = tmp_node;
        *next_byte_idx = tmp_nxt_idx;
        return cnt;
    }
    else
    {
        return NXT_FIELD_NOMORE;
    }
}

buf_chain_t *new_buf()
{
    buf_chain_t *new;

    new = (buf_chain_t *)malloc(sizeof(buf_chain_t));
   
    if(new == NULL)
        return NULL;

    new->next = NULL;
    new->len = 0;

    return new;
}

// append a new buf to the current buf chain
int extend_buf(buf_chain_t **curr_buf)
{
    assert(*curr_buf);
    buf_chain_t *new;

    new = (buf_chain_t *)malloc(sizeof(buf_chain_t));
   
    if(new == NULL)
        return -1;

    new->next = NULL;
    new->len = 0;
    (*curr_buf)->next = new;
    *curr_buf = new;
    return 0;
}

// reclaim buffer memory to the deliminator.
// the buffer identified by the delim is excluded (will not be recycled)
void free_buf(buf_chain_t **head, buf_chain_t *delim)
{
    buf_chain_t *to_free, *p;
    to_free = *head;
    while(to_free != delim)
    {
        p = to_free;
        to_free = to_free->next;
        free(p);
    }
    *head = delim;
}

