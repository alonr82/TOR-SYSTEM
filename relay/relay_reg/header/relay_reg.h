#ifndef RELAY_REG_H
#define RELAY_REG_H

#include <string.h>
#include "sock_utilities.h"
#include "common.h"
#include "relay_manager.h"
#include "relay_listen_tor.h"

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
    uint16_t relay_port;
}relay_signup_request_t;

typedef struct 
{
    uint32_t relay_id;
}relay_signout_request_t;

typedef struct
{
    relay_request_e request_type;
    relay_signout_request_t signout_request;
    relay_signup_request_t signup_request;
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
    relay_signup_response_t signup_response;
    relay_signout_response_t signout_response;
}relay_req_res_t;


/**
 * @brief this function connects to the directory server and signs up the relay
 * 
 * @param dir_cfg_path 
 * @param relay_sock_fd this will be set to the listening socket fd for the relay
 * @return relay_req_res_t* 
 */
relay_req_res_t* relay_connect_only(const char* dir_cfg_path, int *relay_sock_fd);

/**
 * @brief this function connects to the directory server and signs out the relay
 * 
 * @param dif_cfg path to the directory server configuration file
 * @param signup_response the signup response from the directory server
 * @return true if signout was successful, false otherwise
 */

bool relay_connect_signout(const char *dif_cfg,relay_signup_response_t * signup_response);

#endif