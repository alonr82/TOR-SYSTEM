#define _POSIX_C_SOURCE 200112L
#include "sock_utilities.h"

#define _POSIX_C_SOURCE 200112L
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
/*TODO: fix func*/
int connect_server(const server_config_metadata_t* config)
{
    int retval;
    if (config == NULL)
    {
        retval = FAILURE;
    }
    else
    {
        struct addrinfo hints;
        struct addrinfo *res = NULL, *ptr = NULL;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family   = AF_INET;      
        hints.ai_socktype = SOCK_STREAM;
        char ip_str[INET_ADDRSTRLEN];
        snprintf(ip_str, sizeof(ip_str), "%u.%u.%u.%u",(unsigned)config->ip_server[0],(unsigned)config->ip_server[1],
                (unsigned)config->ip_server[2],(unsigned)config->ip_server[3]);
        char port_str[6]; 
        snprintf(port_str, sizeof(port_str), "%u", (unsigned)config->port_server);
        int err = getaddrinfo(ip_str, port_str, &hints, &res);
        if (err != 0)
        {
            retval = FAILURE;
        }
        else
        {
            for (ptr = res; ptr != NULL; ptr = ptr->ai_next)
            {
                retval = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
                if (retval < 0)
                    continue;

                if (connect(retval, ptr->ai_addr, ptr->ai_addrlen) == 0)
                {
                    break;
                }
                close(retval);
                retval = -1;
            }
        }
        freeaddrinfo(res);
    }
    return retval;
}
