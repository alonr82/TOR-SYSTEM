#ifndef RELAY_RUN_TIME_H
#define RELAY_RUN_TIME_H

#include <pthread.h>
#include <stdint.h>
#include <stdbool.h>
#include "common.h"
#include "sock_utilities.h"
#include "tor_protocol.h"
#include "relay_reg.h"

/**
 * @brief this function starts the relay's main runtime operations.
 * 
 * @param user represents the connected client's user descriptor.
 */
static void relay_client_callback(user_descriptor_t* user);


/**
 * @brief this function runs the relay accept loop in a separate thread.
 * 
 * @param _ 
 * @return void* 
 */
static void* relay_accept_loop_func(void* _);

/**
 * @brief this function runs the relay operations.
 * 
 * @param dir_cfg_path path to the diretory config file
 * @return true 
 * @return false 
 */
bool run_relay(const char * dir_cfg_path);

#endif 