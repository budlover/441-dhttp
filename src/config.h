#ifndef _CONFIG_H_
#define _CONFIG_H_

#include <stdint.h>

#define OBJ_NAME_LEN 9
#define MAX_FILE_PATH 255

#define NODE_INFO_INVALID 0
#define NODE_INFO_VALID 1

#define ENTRY_EXISTS    -2

#define IP_LEN 15
typedef struct file_entry
{
    char file_name[OBJ_NAME_LEN + 1];
    char file_path[MAX_FILE_PATH + 1];
    struct file_entry *next; 
} file_entry_t;

typedef struct node_info
{
    uint32_t nid;
    int rd_port;
    int local_port;
    int server_port;
    char ip[IP_LEN + 1];
    int is_valid;
    struct node_info *next;
} node_info_t;

int load_localfile(const char *listfile_path);
int load_neighbor_nodes(const char *conf_file);
int add_localfile(const char *file_name, const char *file_path);

void invalid_nb(node_info_t *nb);
void valid_nb(node_info_t *nb);

void dbg_print_nb_nodes();
void dbg_print_lfile();
#endif
