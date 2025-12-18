#ifndef sock_UTILITIES_H
#define sock_UTILITIES_H

#include "server/configurations/header/dir_server_config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#define STR_PORT_SIZE 6

typedef struct 
{
    uint32_t fd;
    socklen_t addr_len;
    struct sockaddr_storage addr;
}user_descriptor_t;


/**
 * @brief this function creates and binds a socket based on the given server configuration
 * 
 * @param config pointer to the server configuration structure
 * @return int the socket file descriptor if successful, -1 otherwise
 */
int create_and_bind(const server_config_metadata_t *config);



/**
 * @brief this function handles the accept loop for the socket
 * 
 * @param server_fd 
 * @param connection_handler pointer to the function that handles the connection
 */
void accept_loop(int server_fd, void (*connection_handler)(user_descriptor_t* user));




#endif