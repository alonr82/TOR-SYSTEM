#ifndef RELAY_REG_H
#define RELAY_REG_H

#include <string.h>
#include "sock_utilities.h"
#include "common.h"
#include "relay_manager.h"
#include "relay_listen_tor.h"

#define INPUT_SIZE 256


/**
 * @brief this function connects to the directory server and signs up the relay
 * 
 * @param dir_cfg_path 
 * @param relay_sock_fd this will be set to the listening socket fd for the relay
 * @return relay_req_res_t* 
 */
relay_req_res_t* relay_connect_only(const char* dir_cfg_path, int *relay_sock_fd, const uint8_t identity_pub[TOR_ID_PUB_LEN]);

/**
 * @brief this function connects to the directory server and signs out the relay
 * 
 * @param dif_cfg path to the directory server configuration file
 * @param signup_response the signup response from the directory server
 * @return true if signout was successful, false otherwise
 */

bool relay_connect_signout(const char *dif_cfg,relay_signup_response_t * signup_response);

#endif