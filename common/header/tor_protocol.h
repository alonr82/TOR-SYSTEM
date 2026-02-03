#ifndef TOR_PROTOCOL_H
#define TOR_PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>
#include <arpa/inet.h> 
#include "common.h"
#define TOR_MSG_SIZE 4096
#define CIRCUIT_LEN 3

typedef enum
{
    CLIENT_REQUEST = 1,
    RELAY_REQUEST
}request_type_e;

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

typedef enum
{
    relay_success = 0,
    relay_error
}relay_code_e;

typedef enum
{
    RELAY_REG_SIGNUP = 0,
    RELAY_REG_SIGNOUT
} relay_request_e;

typedef struct
{
    uint16_t relay_port;
}relay_signup_request_t;

typedef struct 
{
    uint32_t relay_id;
}relay_signout_request_t;

typedef struct
{
    relay_request_e request_type;
    union 
    {
        relay_signout_request_t signout_request;
        relay_signup_request_t signup_request;
    }request_details_u;
}relay_req_t;


typedef struct
{
    bool status;
}relay_signout_response_t;

typedef struct 
{
    uint32_t relay_id;
    bool status;
}relay_signup_response_t;

typedef struct 
{
    relay_request_e request_type;
    union 
    {
        relay_signup_response_t signup_response;
        relay_signout_response_t signout_response;
    }responese_details_u;
}relay_req_res_t;

typedef struct 
{
    uint32_t guard_index;
    uint32_t middle_index;
    uint32_t exit_index;
}create_circuit_req_t;

typedef struct
{
    request_type_e request_type;
    union 
    {
        create_circuit_req_t create_circuit_req;
        relay_req_t relay_req;
    }request_u;
    
}request_t;

#endif
