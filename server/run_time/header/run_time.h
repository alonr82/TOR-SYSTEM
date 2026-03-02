#ifndef RUN_TIME_H
#define RUN_TIME_H

#include "sock_utilities.h"
#include "relay_reg.h"
#include <string.h>
#include <sys/socket.h>
#include "run_time.h"
#include "relay_manager.h"
#include <pthread.h>
#include "tor_protocol.h"

#define MAX_RELAY_BATCH_SIZE 10
#define INPUT_SIZE 256
typedef enum
{
    running_status_success = 0,
    running_status_failure
}server_running_status_e;




/**
 * @brief this function starts to get connections for the directory server
 * 
 * @param config_file a path to the directory config file
 * @return server_running_status_e 
 */
server_running_status_e run_dir_server(const char *config_file);

/**
 * @brief Get the data from req object
 * 
 * @param user relay user descriptor
 * @param request this is the request from the relay
 * @return relay_decript_t* 
 */
relay_decript_t* get_data_from_req(user_descriptor_t* user, relay_req_t* request);

#endif
