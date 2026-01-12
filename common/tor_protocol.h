#ifndef TOR_PROTO_H
#define TOR_PROTO_H

#include <stdint.h>
#include <stdbool.h>
#include <arpa/inet.h> 

#define TOR_MSG_SIZE 4096

typedef enum {
    TOR_MSG_EXTEND = 1,
    TOR_MSG_DATA   = 2
} tor_msg_type_e;

typedef struct 
{
    uint8_t  type;   
    uint16_t payload_len; 
} __attribute__((packed)) tor_header_t;

typedef struct  
{
    tor_header_t header;
    uint8_t   payload[TOR_MSG_SIZE - sizeof(tor_header_t)];
} __attribute__((packed)) tor_msg_t;

typedef struct 
{
    uint32_t ip_v4; 
    uint16_t port; 
} __attribute__((packed))  tor_extend_t;



#endif
