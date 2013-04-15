#ifndef _LSA_H_
#define _LSA_H_

#include <stdint.h>
#include <arpa/inet.h>

#define LSA_MAX_TTL 32

#define LINK_ENTRY_LEN 4
#define OBJ_ENTRY_LEN 10

#define LSA_TYPE_AD 0 
#define LSA_TYPE_ACK 1 
#define LSA_TYPE_SEQ 2

#define LSA_VALID    0
#define LSA_INVALID  1

// the payload of lnk entry and obj entry is immediately after this instance
typedef struct lsa
{
    struct lsa *next;
    int is_valid;
    long time_left;
    uint32_t ann_size;  //announcement size, start from ver field

    uint8_t ver;
    uint8_t ttl;
    uint16_t type;
    uint32_t snder_id;      //the key in the hashtable
    uint32_t seq_num;
    int32_t num_lnk_entr;
    int32_t num_obj_entr;
} lsa_t;

typedef struct lnk_entr
{
    uint32_t nid;
} lnk_entr_t;

typedef struct obj_entr
{
    char obj[OBJ_ENTRY_LEN];
} obj_entr_t;


typedef struct lsa_header
{
    uint8_t ver;
    uint8_t ttl;
    uint16_t type;
    uint32_t snder_id;      //the key in the hashtable
    uint32_t seq_num;
    int32_t num_lnk_entr;
    int32_t num_obj_entr;
} lsa_header_t;

typedef struct pend_lsa
{
    struct pend_lsa *next;

    uint32_t snder_id;
    uint32_t seq_num;
    struct sockaddr_in addr;

    long time_left;
    int is_acked;   
    int type;   //ad, ack or seq

    int payload_size;
    char payload[1];
} pend_lsa_t;




lsa_t *init_lsa();
lsa_t *reload_local_lsa();
lsa_t *update_lsa_entr(lsa_header_t *head, char *body, size_t body_sz);
int  update_seq_num(uint32_t snder_id, uint32_t seq_num);
void lsa_update_retran_timeout(pend_lsa_t *pd_lsa);

void set_retran_timeout_interval(long interval);
void set_lsa_timeout_interval(long interval);
void set_nb_timeout_interval(long interval);

pend_lsa_t *lsa_create_ack_ann(lsa_header_t *head, struct sockaddr_in addr);
pend_lsa_t *lsa_create_seq_ann(lsa_header_t *head, uint32_t seq_num,
                               struct sockaddr_in addr);

pend_lsa_t *lsa_create_ad_ann(lsa_t *lsa, struct sockaddr_in addr);

void dbg_print_stored_lsa(lsa_t *lsa_h);
#endif
