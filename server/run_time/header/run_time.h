#ifndef RUN_TIME_H
#define RUN_TIME_H

#include "sock_utilities.h"
#include "relay_reg.h"

#define INPUT_SIZE 256
typedef enum
{
    running_status_success = 0,
    running_status_failure
}server_running_status_e;


/**
 * @brief this function reads commands
 * 
 */
static void run_commands();

/**
 * @brief this function is the callback for new relays/clients
 * 
 * @param user 
 */
static bool client_callback(user_descriptor_t* user);

/**
 * @brief this function is the accept loop thread function
 * 
 */
static void* accept_loop_func(void* _);

/**
 * @brief this function starts to get connections for the directory server
 * 
 * @param config_file a path to the directory config file
 * @return server_running_status_e 
 */
server_running_status_e run_dir_server(const char *config_file);

/**
 * @brief this function extracts relay data from the request
 * 
 */
relay_decript_t* get_data_from_req(user_descriptor_t* user, relay_request_t* request);

#endif
