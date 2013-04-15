#ifndef _H_BUF_CTRL_
#define _H_BUF_CTRL_

#define BUFBLK_SIZE 2048

#define NXT_FIELD_NOMORE -2
#define NXT_FIELD_FAILED -1

#define NXT_BYTE_NO_MORE -2
#define GET_BYTES_FAILED -1

typedef struct buf_chain
{
    struct buf_chain *next;
    unsigned len;               //indicates the valid data len
    char payload[BUFBLK_SIZE]; //
} buf_chain_t;

int get_bytes(buf_chain_t **curr_buf_node, int *next_byte_idx, char *buf, int size);
int get_next_field(buf_chain_t **curr_buf_node, int *next_byte_idx, char *buf, int buf_size);

buf_chain_t *new_buf();
int extend_buf(buf_chain_t **curr_buf);
void free_buf(buf_chain_t **head, buf_chain_t *delim);

#endif
