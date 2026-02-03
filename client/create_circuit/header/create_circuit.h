#ifndef CREATE_CIRCUIT_H
#define CREATE_CIRCUIT_H

#include "tor_protocol.h"
#include "sock_utilities.h"
#include "relay_reg.h"
#include "dir_server_config.h"
#include <time.h>
#define MAX_RELAY_BATCH_SIZE 10

/**
 * @brief this function connects to the directory server and gets 
 * a list of the relays available
 * 
 * @param dir_cfg 
 * @return true 
 * @return false 
 */
bool connect_to_dir_server(const char *dir_cfg);








#endif