#include <stdlib.h>
#include "dir_server_config.h"
#include <string.h>

bool fetch_server_ip_config(server_config_metadata_t *server_config, char* line)
{
    bool retval = true;
    if(strncmp(line,"ip=",IP_TITLE_LEN) == 0)
    {
        char* ip_str = line + IP_TITLE_LEN;
        unsigned int num1,num2,num3,num4;
        if(sscanf(ip_str, "%u.%u.%u.%u", &num1, &num2, &num3, &num4) == 4)
        {
            if(num1> 255 || num2> 255 ||num3> 255 || num4 > 255)
            {
                printf("Invalid IP address in server config file\n");
                retval = false;
            }
            else
            {
                server_config->ip_server[0] = (uint8_t)num1;
                server_config->ip_server[1] = (uint8_t)num2;
                server_config->ip_server[2] = (uint8_t)num3;
                server_config->ip_server[3] = (uint8_t)num4;
            }
        }
        else
        {
            retval = false;
        }
    }
    else
    {
        retval = false;
    }
    return retval;
}

bool fetch_server_port_config(server_config_metadata_t *server_config, char *line)
{
    bool retval = true;
    if(strncmp(line,"port=",PORT_TITLE_LEN) == 0)
    {
        char *port_start = line + PORT_TITLE_LEN;
        unsigned int port;
        sscanf(port_start,"%u",&port);
        if(port > 65535)
        {
            retval = false;
        }
        else
        {
            server_config->port_server = (uint16_t)port;
        }
    }
    else
    {
        retval = false;
    }
    return retval;
}


server_config_metadata_t* fetch_server_config(char* filepath)
{
    bool status = true;
    FILE *file_ptr = fopen(filepath,"r");
    if(file_ptr == NULL)
    {
        fprintf(stderr,"Error opening server config file\n");
        status = false;
    }
    else
    {
        server_config_metadata_t* server_config = calloc(1,sizeof(server_config_metadata_t));
        char buffer[MAX_FILE_LEN];
        fgets(buffer,MAX_FILE_LEN,file_ptr);
        if(fetch_server_ip_config(server_config,buffer))
        {
            fgets(buffer,MAX_FILE_LEN,file_ptr);
            if(fetch_server_port_config(server_config,buffer))
            {
                fclose(file_ptr);
            }
            else
            {
                free(server_config);
                fprintf(stderr,"error fetching port from configuration file\n");
                server_config = NULL;
                status = false;
            }
        }
        else
        {
            free(server_config);
            fprintf(stderr,"error fetching ip from configuration file\n");
            server_config = NULL;
            status= false;
        }
        if(status)
        {
            return server_config;
        }
        else
        {
            free(server_config);
        }
    }
    if(file_ptr != NULL)
    {
        fclose(file_ptr);
    }
    return NULL;
    
}

bool free_server_config(server_config_metadata_t *config)
{
    bool retval = true;
    if(config != NULL)
    {
        free(config);
    }
    else
    {
        retval = false;
    }
    return retval;
}