#ifndef sock_UTILITIES_H
#define sock_UTILITIES_H

#include "dir_server_config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <pthread.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include "tor_protocol.h"

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


/**
 * @brief this function connects to a remote server based on the given configuration
 * 
 * @param config pointer to the server configuration structure
 * @return int the socket file descriptor if successful, -1 otherwise
 */
int connect_server(const server_config_metadata_t* config);

/**
 * @brief this function is a wrapper for the connection handler
 * 
 * @param arg pointer to the user descriptor
 * @return void* 
 */
static void* connection_handler_wrapper(void* arg);

/**
 * @brief this function reads an exact number of bytes from a socket
 * 
 * @param fd 
 * @param buf 
 * @param expected_size 
 * @return true 
 * @return false 
 */
static bool read_exact(int fd, void *buf, size_t expected_size);

/**
 * @brief this function writes an exact number of bytes to a socket
 * 
 * @param fd 
 * @param buf 
 * @param expected_size 
 * @return true 
 * @return false    
 */
static bool write_exact(int fd, const void *buf, size_t expected_size);

/**
 * @brief this function receives a TOR message from a socket
 * @param fd 
 * @param input 
 * @return true 
 * @return false 
 */
bool tor_recv_msg(int fd, tor_msg_t *input);

/**
 * @brief this function sends a TOR message to a socket
 * 
 * @param fd 
 * @param msg 
 * @return true 
 * @return false 
 */
bool tor_send_msg(int fd, const tor_msg_t *msg);


/**
 * @brief this function validates a TOR message
 * 
 * @param message 
 * @return true 
 * @return false 
 */
static  bool tor_msg_valid(const tor_msg_t *message);

/**
 * @brief Set the socket nonblock object
 * 
 * @param sock_fd 
 */
void set_socket_nonblock(int sock_fd);
#endif