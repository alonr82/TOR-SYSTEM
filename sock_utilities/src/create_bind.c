#define _POSIX_C_SOURCE 200112L
#include "sock_utilities.h"

int create_and_bind(const server_config_metadata_t *config)
{
    int retval = SUCCESS;
    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof hints);
    hints.ai_family   = AF_UNSPEC;      
    hints.ai_socktype = SOCK_STREAM;   
    hints.ai_flags    = AI_PASSIVE; 
    char port_str[STR_PORT_SIZE];
    snprintf(port_str, sizeof(port_str),"%u",ntohs(config->port_server));
    if (getaddrinfo(NULL, port_str, &hints, &res) != 0) 
    {
        printf("Failed to getaddr on %s\n", port_str);
        retval = FAILURE;
    }
    else
    {
        retval = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if (retval == -1)
        {
            printf("Failed to create socket %s\n", port_str);
            freeaddrinfo(res);
            retval = FAILURE;
        }
        else
        {
            int optval = 1;
            setsockopt(retval, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
            if (bind(retval, res->ai_addr, res->ai_addrlen) == -1)
            {
                printf("Failed to bind on %s\n", port_str);
                freeaddrinfo(res);
                retval = FAILURE;
            }
        }
    }
    freeaddrinfo(res);
    return retval;
}