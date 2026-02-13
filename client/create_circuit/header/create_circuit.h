#ifndef CREATE_CIRCUIT_H
#define CREATE_CIRCUIT_H

#include "tor_protocol.h"
#include "sock_utilities.h"
#include "relay_reg.h"
#include "dir_server_config.h"
#include "tor_crypto.h"
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

int circuit_connect_guard(circuit_t * circuit);

bool circuit_extend(circuit_t *circuit);

bool build_default_circuit(circuit_t *circuit, relay_decript_t *relay_list);






#endif