#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <stdbool.h>

#define IP4_SIZE 4
#define IP6_SIZE 16
#define SUCCESS 0
#define FAILURE -1
#define SERVER_MSG_SIZE 4096


typedef struct
{
    uint32_t relay_id;
    uint8_t relay_ip[IP6_SIZE];
    uint8_t ip_type;
    uint16_t relay_port;
    bool is_active;
}relay_decript_t;

typedef struct 
{
    relay_decript_t descriptor;
    uint32_t relay_id;
}relay_data_t;

typedef struct msg_server_buffer {
    uint8_t data[SERVER_MSG_SIZE];
} msg_server_buffer_t;





#endif