#include "relay_server_config.h"
#include <stdio.h>
#include <stdlib.h>

bool parse_args(const int argc, const char** argv, relay_config_metadata_t** relay_config)
{
    bool retval = true;
    bool port_found = false;
    bool server_cfg_found = false;

    *relay_config = (relay_config_metadata_t*)calloc(1, sizeof(relay_config_metadata_t));
    if (!*relay_config)
    {
        retval = false;
    }
    else
    {
        for (int index = 1; index < argc; index++)
        {
            if ((strcmp(argv[index], "-p") == 0 || strcmp(argv[index], "--port") == 0) && (index + 1 < argc) && !port_found)
            {
                index++;
                char *endptr = NULL;
                unsigned long port_val = strtoul(argv[index], &endptr, 10);
                if (endptr == argv[index] || *endptr != '\0' || port_val == 0 || port_val > 65535)
                {
                    retval = false;
                    break;
                }
                (*relay_config)->relay_conf.port_server = (uint16_t)port_val;
                (*relay_config)->relay_conf.ip_server[0] = 0;
                (*relay_config)->relay_conf.ip_server[1] = 0;
                (*relay_config)->relay_conf.ip_server[2] = 0;
                (*relay_config)->relay_conf.ip_server[3] = 0;
                port_found = true;
            }
            else if (!server_cfg_found)
            {
                server_config_metadata_t* dir_cfg_check = fetch_server_config((char *)argv[index]);
                if(!dir_cfg_check)
                {
                    retval = false;
                }
                else
                {
                    (*relay_config)->dir_conf = *dir_cfg_check;
                    free(dir_cfg_check);
                    server_cfg_found = true;
                }
            }
        }

        if (!port_found || !server_cfg_found)
        {
            retval = false;
        }

        if (!retval)
        {
            free(*relay_config);
            *relay_config = NULL;
        }
    }

    return retval;
}

// void free_relay_config(relay_config_metadata_t* relay_config)
// {
//     free_server_config(&relay_config->relay_server_cfg);
//     free_server_config(&relay_config->dir_server_cfg);
// }