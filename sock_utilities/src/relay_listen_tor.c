#include "relay_listen_tor.h"

int relay_listen_on_any_port(uint16_t *out_port)
{
    int retval = socket(AF_INET, SOCK_STREAM, 0);
    if (retval < 0) 
    {
        perror("socket");
        retval = FAILURE;

    }
    else
    {
        int opt = 1;
        if (setsockopt(retval, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) 
        {
            perror("setsockopt");
            close(retval);
            retval = FAILURE;
        }
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_ANY); 
        addr.sin_port = htons(0); 
                     
        
        if (bind(retval, (struct sockaddr*)&addr, sizeof(addr)) < 0) 
        {
            perror("bind");
            close(retval);
            retval = FAILURE;
        }
        else
        {
            socklen_t len = sizeof(addr);
            if (getsockname(retval, (struct sockaddr*)&addr, &len) < 0) 
            {
                perror("getsockname");
                close(retval);
                retval = FAILURE;
            }
            else
            {
                uint16_t chosen_port = ntohs(addr.sin_port);
                printf("relay_listen_tor: listening on port %u\n", chosen_port);
                if (out_port) 
                {
                    *out_port = chosen_port;
                }
                if (listen(retval, MAX_CONNECTIONS) < 0) 
                {
                    perror("listen");
                    close(retval);
                    retval = FAILURE;
                }
                else
                {
                    set_socket_nonblock(retval);
                }
            }
        }
    }
    return retval;
}