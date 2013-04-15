#include <stdlib.h>
#include <stdio.h>
#include <memory.h>

#include "config.h"
#include "ht.h"
#include "str_helper.h"

#define LFILE_HT_BUCKET 100
#define NEIGHBOR_HT_BUCKET 100

#define LFILE_LINE_LEN (OBJ_NAME_LEN + MAX_FILE_PATH + 2)
#define NB_NODE_LINE_LEN 50


hash_info_t g_lfile_ht;
hash_info_t g_nb_nodes_ht;
uint32_t g_nid;
int g_lc_port;    //local port
int g_rd_port;    //routing daemon port
int g_server_port; //server port

static file_entry_t *lfile_bucket[LFILE_HT_BUCKET];
static node_info_t *nodeinf_bucket[NEIGHBOR_HT_BUCKET];

// the "node*.files" path
static char localfile_path[LFILE_LINE_LEN + 1];

/* load mapping betweeen obj name and real file path */
int load_localfile(const char *listfile_path)
{
    char buff[LFILE_LINE_LEN + 1];
    FILE *fp = NULL;
    file_entry_t *p = NULL;

    // init the hash table for the mapping
    g_lfile_ht.table_base = (void **)lfile_bucket;
    g_lfile_ht.bucket_size = LFILE_HT_BUCKET; 
    g_lfile_ht.key_to_idx = str_to_idx; 
    g_lfile_ht.cmp_key = cmpstr;

    fp = fopen(listfile_path, "r");
    if(NULL == fp)
        return -1;

    // load the files
    while(NULL != fgets(buff, LFILE_LINE_LEN + 1, fp))
    {
        char *str;
        p = (file_entry_t *)malloc(sizeof(file_entry_t));
        if(NULL == p)
            goto err;
        
        str = strtok(buff, " \n");
        if(NULL == str)
           goto err;
        if(strlen(str) > OBJ_NAME_LEN)
            goto err; 
        strcpy(p->file_name, str);

        str = strtok(NULL, " \n");
        if(NULL == str)
           goto err;
        if(strlen(str) > MAX_FILE_PATH)
            goto err; 
        strcpy(p->file_path, str);
        
        HT_INSERT(&g_lfile_ht, file_entry_t, p->file_name, p);
    }
    
    fclose(fp);

    strcpy(localfile_path, listfile_path);
    return 0;

err:
    if(p)
        free(p);

    if(fp)
        fclose(fp);
    return -1;
}

void invalid_nb(node_info_t *nb)
{
    nb->is_valid = NODE_INFO_INVALID;
}

void valid_nb(node_info_t *nb)
{
    nb->is_valid = NODE_INFO_VALID;
}

int load_neighbor_nodes(const char *conf_file)
{
    char buff[NB_NODE_LINE_LEN + 1];
    FILE *fp = NULL;
    node_info_t *p = NULL;

    // init the hash table for neighbor nodes info
    g_nb_nodes_ht.table_base = (void **)nodeinf_bucket;
    g_nb_nodes_ht.bucket_size = NEIGHBOR_HT_BUCKET;
    g_nb_nodes_ht.key_to_idx = uint_to_idx; 
    g_nb_nodes_ht.cmp_key = cmp_int;

    fp = fopen(conf_file, "r");
    if(NULL == fp)
        goto err;

    while(fgets(buff, NB_NODE_LINE_LEN + 1, fp))
    {
        char *str;

        p = (node_info_t *)malloc(sizeof(node_info_t));
        if(!p)
            goto err;
         
        str = strtok(buff, " \n");
        if(!str)
            goto err;
        if(0 != str_to_uint32(str, &p->nid))
            goto err;
        
        str = strtok(NULL, " \n");
        if(!str)
            goto err;
        if(strlen(str) > IP_LEN)
           goto err; 
        strcpy(p->ip, str);

        str = strtok(NULL, " \n");
        if(!str)
            goto err;
        if(0 != str_to_num(str, &p->rd_port))
            goto err;
    
        str = strtok(NULL, " \n");
        if(!str)
            goto err;
        if(0 != str_to_num(str, &p->local_port))
            goto err;

        str = strtok(NULL, " \n");
        if(!str)
            goto err;
        if(0 != str_to_num(str, &p->server_port))
            goto err;
        p->is_valid = NODE_INFO_VALID;
        HT_INSERT(&g_nb_nodes_ht, node_info_t, p->nid, p);
    }

    fclose(fp);
    return 0;

err:
    if(p)
        free(p);
    if(fp)
        fclose(fp);
    return -1;
}

void dbg_print_lfile()
{
    int i;
    file_entry_t *p;
    for(i = 0; i < g_lfile_ht.bucket_size; i++)
    {
        p = g_lfile_ht.table_base[i];
        while(p)
        {
            printf("file name is %s\n", p->file_name);
            printf("file path is %s\n", p->file_path);
            p = p->next;
        }
    }
}

void dbg_print_nb_nodes()
{
    int i;
    node_info_t *p;
    for(i = 0; i < g_nb_nodes_ht.bucket_size; ++i)
    {
        p = g_nb_nodes_ht.table_base[i];
        while(p)
        {
            printf("node id: %u\n", p->nid);
            printf("ip: %s\n", p->ip);
            printf("rd port: %d\n", p->rd_port);
            printf("local port: %d\n", p->local_port);
            printf("server port: %d\n", p->server_port);
            printf("*******************************************\n");
            p = p->next;
        }
    }
}

/* add new obj and file mapping. Will write back to the config files */
int add_localfile(const char *file_name, const char *file_path)
{
    
    file_entry_t *s;
    HT_FIND(&g_lfile_ht, file_entry_t, file_name, file_name, s);
    if(s) // mapping already exists
    {
        return ENTRY_EXISTS; 
    }

    file_entry_t *p = (file_entry_t *)malloc(sizeof(file_entry_t));
    if(NULL == p)
        goto err;

    strcpy(p->file_name, file_name);
    strcpy(p->file_path, file_path);
    
    FILE *fp;
    fp = fopen(localfile_path, "a");
    if(!fp)
        goto err;

    fwrite(file_name, 1, strlen(file_name), fp);
    fwrite(" ", 1, 1, fp);
    fwrite(file_path, 1, strlen(file_path), fp);
    fwrite("\n", 1, 1, fp);
    fclose(fp);
    
    // only insert after write to file success
    HT_INSERT(&g_lfile_ht, file_entry_t, p->file_name, p);

    return 0;
err:
    if(p)
        free(p);
    return -1;
}

