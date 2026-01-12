#ifndef RELAY_LISTEN_TOR_H
#define RELAY_LISTEN_TOR_H

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include "common.h"

#define MAX_CONNECTIONS 10

/**
 * @brief this function creates a listening socket on any available port on the loopback interface.
 * 
 * @param out_port pointer to the port number chosen by the OS, can be NULL if not needed.
 * @return int 
 */
int relay_listen_on_any_port(uint16_t *out_port);


#endif 