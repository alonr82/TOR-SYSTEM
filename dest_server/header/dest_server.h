#ifndef DEST_SERVER_H
#define DEST_SERVER_H

#include "tor_protocol.h"
#include "sock_utilities.h"

#define DEST_SERVER_PORT 8080
#define BACKLOG_NUM 10

/**
 * @brief this function creates, binds, and listens on a socket for the destination server
 * 
 * @return int socket file descriptor
 */
int dest_listen_socket();

/**
 * @brief this function accepts incoming connections in a loop
 * 
 * @param dest_fd 
 */
void dest_accept_loop(int dest_fd);

/**
 * @brief this function handles incoming messages
 * 
 * @param client_fd 
 * @return true 
 * @return false 
 */
bool dest_handle_message(int client_fd);

/**
 * @brief this function handles client connecitions in seperate threads
 * 
 * @param arg 
 * @return void* 
 */
void * dest_thread(void * arg);

/**
 * @brief this function runs the destination server
 * 
 * @return true 
 * @return false 
 */
bool run_server();
#endif