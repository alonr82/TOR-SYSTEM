#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <stdbool.h>
#include <time.h>

#define IP4_SIZE 4
#define IP6_SIZE 16
#define SUCCESS 0
#define FAILURE -1
#define SERVER_MSG_SIZE 4096
#define DEST_SERVER_PORT 8080
#define PUB_LEN 32

typedef struct
{
    uint32_t relay_id;
    uint8_t relay_ip[IP6_SIZE];
    uint8_t ip_type;
    uint16_t relay_port;
    bool is_active;
    uint8_t identify_pub[PUB_LEN];
    uint32_t assigned_count;
    bool is_malicious;
    bool is_verified_historically;
    bool is_filtered;
    bool eligible_guard;
    bool is_guard;
    bool is_exit;
    uint32_t trust_score;
    uint32_t source_ip_v4;
    uint32_t same_source_count;
    time_t registration_time;
    uint32_t uptime_seconds;
    uint64_t first_seen_unix_sec;
}relay_decript_t;

typedef struct 
{
    relay_decript_t descriptor;
    uint32_t relay_id;
}relay_data_t;

typedef struct msg_server_buffer 
{
    uint8_t data[SERVER_MSG_SIZE];
} msg_server_buffer_t;

#endif