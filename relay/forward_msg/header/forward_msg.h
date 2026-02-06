#ifndef FORWARD_MSG_H
#define FORWARD_MSG_H

#include <stdio.h>
#include <stdlib.h>
#include <poll.h>
#include "tor_protocol.h"
#include "sock_utilities.h"

#define TWO_SOCKETS 2

typedef struct
{
    int last_fd;
    int next_fd;
}session_t;

/**
 * @brief this function connets the relay to the next dest and saves the fd
 * 
 * @param session struct that include both fds
 * @param msg the message we recived
 * @return true 
 * @return false 
 */
bool extend_connection(session_t *session, tor_msg_t *msg);

/**
 * @brief this function responsible for moving the data between
 * the sockets
 * 
 * @param last_fd the one "behind" the current relay 
 * @param next_fd the one "after" the current relay
 */
void forward_messages(int last_fd, int next_fd, volatile bool *run_flag);

/**
 * @brief this function connects the exit relay to the dest server
 * 
 * @return int fd
 */
int connect_to_dest_server(void);

#endif