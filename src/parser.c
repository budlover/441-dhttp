#define DEBUG
#include <assert.h>

#include "parser.h"
#include "memory.h"
#include "config.h"
#include "buf_ctrl.h"
#include "str_helper.h"
#include "dbg_helper.h"

#define METHOD_MAX_LEN 7

#define METHOD_NUM 2
static const char *method_list[] = {"ADDFILE", "GETRD"};

void init_parser(buf_chain_t *node, parser_info_t *parser_inf)
{
    parser_inf->status = PARSER_STATUS_NEW; 
    parser_inf->next_byte_idx = 0;
    parser_inf->curr_node = node;
}

void update_parser_info(parser_info_t *parser_inf)
{
    parser_inf->status = PARSER_STATUS_NEW;
}

void init_req_info(req_info_t *req_inf)
{
    req_inf->status = REQUEST_STATUS_OK;
    req_inf->field1_len = 0;
    req_inf->field2_len = 0;
}

void update_req_info(req_info_t *req_inf)
{
    req_inf->status = REQUEST_STATUS_OK;
    req_inf->field1_len = 0;
    req_inf->field2_len = 0;
}

static int parse_method(parser_info_t *parser_inf, req_info_t *req_inf)
{
    char buff[METHOD_MAX_LEN + 1];
    int ret;
    int idx;

    ret = get_next_field(&parser_inf->curr_node, &parser_inf->next_byte_idx,
                         buff, METHOD_MAX_LEN + 1);

    if(ret == NXT_FIELD_FAILED)
    {
        parser_inf->status = PARSER_STATUS_ERROR;
        req_inf->status = REQUEST_STATUS_ERROR_BAD_REQUEST;
        return -1;
    }
    else if(ret == NXT_FIELD_NOMORE)
    {
        return -1;
    }
   
    idx = str_match(method_list, METHOD_NUM, buff, METHOD_MAX_LEN);
    if(-1 == idx)
    {
        parser_inf->status = PARSER_STATUS_ERROR;
        req_inf->status = REQUEST_STATUS_ERROR_BAD_REQUEST;
        return -1;
    }

    req_inf->method = idx;
    parser_inf->status = PARSER_STATUS_METHOD_DONE;  
    return 0;
}

int parse_len(parser_info_t *parser_inf, req_info_t *req_inf)
{
    char buff[10];
    int ret;
    int len;
    
    ret = get_next_field(&parser_inf->curr_node, &parser_inf->next_byte_idx,
                         buff, 10);
    
    if(ret == NXT_FIELD_FAILED)
    {
        parser_inf->status = PARSER_STATUS_ERROR;
        req_inf->status = REQUEST_STATUS_ERROR_BAD_REQUEST;
        return -1;
    }
    else if(ret == NXT_FIELD_NOMORE)
    {
        return -1;
    }

    if(0 != str_to_num(buff, &len))
    {
        parser_inf->status = PARSER_STATUS_ERROR;
        req_inf->status = REQUEST_STATUS_ERROR_BAD_REQUEST;
        return -1;
    }

    switch(parser_inf->status)
    {
    case PARSER_STATUS_METHOD_DONE:
        req_inf->field1_len = len;
        if(len > REQ_FIELD1_MAX_LEN || len <= 0)
        {
            parser_inf->status = PARSER_STATUS_ERROR;
            req_inf->status = REQUEST_STATUS_ERROR_BAD_REQUEST;
            return -1;
        }
        parser_inf->status = PARSER_STATUS_LEN1_DONE;
        break;

    case PARSER_STATUS_FIELD1_DONE:
        req_inf->field2_len = len;
        if(len > REQ_FIELD2_MAX_LEN || len <= 0)
        {
            parser_inf->status = PARSER_STATUS_ERROR;
            req_inf->status = REQUEST_STATUS_ERROR_BAD_REQUEST;
            return -1;
        }
        parser_inf->status = PARSER_STATUS_LEN2_DONE;
        break;

    default:
        assert(0);
    }

    return 0;
}

int parse_data(parser_info_t *parser_inf, req_info_t *req_inf)
{
    //len of field 1 < len of field 2
    char buff[REQ_FIELD2_MAX_LEN + 1];
    int ret;
    int len = 0;

    switch(parser_inf->status)
    {
    case PARSER_STATUS_LEN1_DONE:
        len = req_inf->field1_len;
        break;

    case PARSER_STATUS_LEN2_DONE:
        len = req_inf->field2_len;
        break;

    default:
        assert(0);
    }
    
    ret = get_bytes(&parser_inf->curr_node, &parser_inf->next_byte_idx,
                         buff, len);
    
    if(ret != 0)
    {
        return -1;
    }

    buff[len] = '\0';

    switch(parser_inf->status)
    {
    case PARSER_STATUS_LEN1_DONE:
        strcpy(req_inf->field1, buff);
        break;

    case PARSER_STATUS_LEN2_DONE:
        strcpy(req_inf->field2, buff);
        break;

    default:
        assert(0);
    }

    switch(req_inf->method)
    {
    case REQUEST_METHOD_GETRD:
        parser_inf->status = PARSER_STATUS_DONE;
        break;

    case REQUEST_METHOD_ADDFILE:
        if(parser_inf->status == PARSER_STATUS_LEN1_DONE)
            parser_inf->status = PARSER_STATUS_FIELD1_DONE;
        else //len2 done
            parser_inf->status = PARSER_STATUS_DONE;

        break;

    default:
        assert(0);
    }

    return 0;
}

// return 1 for get full request
// return 0 for not full request
int parse_req(parser_info_t *parser_inf, req_info_t *req_inf)
{
    int ret;
    while(1)
    {
        switch(parser_inf->status)
        {
        case  PARSER_STATUS_NEW:
            ret = parse_method(parser_inf, req_inf);
            break;

        case PARSER_STATUS_METHOD_DONE:  //get field 1 len
        case PARSER_STATUS_FIELD1_DONE:  //get field 2 len
            ret = parse_len(parser_inf, req_inf);
            break;

        case PARSER_STATUS_LEN1_DONE://to parse field 1
        case PARSER_STATUS_LEN2_DONE://to pasee field 2
            ret = parse_data(parser_inf, req_inf);
            break;

        default:
            assert(0);
        }
        if(-1 == ret || parser_inf->status == PARSER_STATUS_DONE)
            break;
    }

    if(parser_inf->status == PARSER_STATUS_DONE ||
       parser_inf->status == PARSER_STATUS_ERROR)
        return 1;

    return 0;
}
