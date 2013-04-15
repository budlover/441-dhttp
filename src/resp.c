#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "resp.h"

static char *err_list[] =
{
"OK",
"Not Found",
"Bad Request",
"Server Internal Error",
"Record Exists"
};

static void escape_html_str(char *buff, const char *str)
{
    if(*str == '/')
    {
        *buff = '\\';
        *(++buff) = '\\';

        ++buff;
        ++str;
    }

    while(*str)
    {
        *buff = *str;
        ++buff;
        ++str;
    }
   *buff = '\0';
}

int fill_local_resp(char *buff, size_t len, const char *file_name, 
                    const char *ip, int server_port,
                    int remote_lc_port, const char *local_file_path,
                    REQUEST_METHOD method, REQUEST_STATUS status)
{
    if(status == REQUEST_STATUS_OK)
    {
        if(method == REQUEST_METHOD_ADDFILE)
        {
            strcpy(buff, "OK");
        }
        else //GETRD
        {
            char num[15];
            strcpy(buff, "OK http://");
            strcat(buff, ip);
            strcat(buff, ":");

            sprintf(num, "%d", server_port);
            strcat(buff, num);

            if(remote_lc_port != -1) //file is not stored locally
            {
                strcat(buff, "/rd/");
                sprintf(num, "%d", remote_lc_port);
                strcat(buff, num);
                strcat(buff, "/");
               // strcat(buff, file_name);
                escape_html_str(buff + strlen(buff), file_name);
            }
            else //file is locally stored
            {
                strcat(buff, "/");
                strcat(buff, local_file_path);
            }
        }
    }
    else //error
    {
        char num_buff[20];
        strcpy(buff, "ERROR ");
        sprintf(num_buff, "%Zu", strlen(err_list[status]));
        strcat(buff, num_buff);
        strcat(buff, " ");
        strcat(buff, err_list[status]);
    }

    return strlen(buff);
}

