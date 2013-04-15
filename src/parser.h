#ifndef _PARSER_H_
#define _PARSER_H_

#include "buf_ctrl.h"

#define REQ_FIELD1_MAX_LEN 9
#define REQ_FIELD2_MAX_LEN 512


typedef enum _PARSER_STATUS
{
    PARSER_STATUS_ERROR = -1,
    PARSER_STATUS_NEW,
    PARSER_STATUS_METHOD_DONE,
    PARSER_STATUS_LEN1_DONE,
    PARSER_STATUS_FIELD1_DONE,
    PARSER_STATUS_LEN2_DONE,
    PARSER_STATUS_DONE
} PARSER_STATUS;

typedef enum _REQUEST_METHOD
{
    REQUEST_METHOD_ADDFILE = 0,
    REQUEST_METHOD_GETRD
} REQUEST_METHOD;

typedef enum _REQUEST_STATUS
{
    REQUEST_STATUS_OK = 0,
    REQUEST_STATUS_ERROR_NOT_FOUND,
    REQUEST_STATUS_ERROR_BAD_REQUEST,
    REQUEST_STATUS_ERROR_SERVER_INTERNAL,
    REQUEST_STATUS_ERROR_RECORD_EXISTS
} REQUEST_STATUS;

typedef struct parser_info
{
    PARSER_STATUS status;
    int next_byte_idx;      // next byte to parse
    buf_chain_t *curr_node;
} parser_info_t;

typedef struct req_info
{
    REQUEST_METHOD method;
    REQUEST_STATUS status;
    int field1_len;
    int field2_len;
    char field1[REQ_FIELD1_MAX_LEN + 1];
    char field2[REQ_FIELD2_MAX_LEN + 1];
} req_info_t;

void init_parser(buf_chain_t *node, parser_info_t *parser_inf);
void init_req_info(req_info_t *req_inf);
int parse_req(parser_info_t *parser_inf, req_info_t *req_inf);
void update_req_info(req_info_t *req_inf);
void update_parser_info(parser_info_t *parser_inf);

#endif
