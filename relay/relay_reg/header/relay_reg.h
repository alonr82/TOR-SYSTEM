#ifndef RELAY_REG_H
#define RELAY_REG_H

#include <string.h>
#include "sock_utilities.h"
#include "common.h"

#define INPUT_SIZE 256

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
    relay_request_e request_type;
    uint16_t relay_port;
}relay_request_t;

typedef struct 
{
    uint32_t relay_id;
    bool status;
}relay_req_response_t;


/**
 * @brief this function connects to the directory server based on the given configuration file
 * 
 */
relay_code_e relay_connect_only(const char* dir_cfg_path);

#endif