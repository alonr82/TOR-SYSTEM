#ifndef RELAY_SERVER_CONFIG_H
#define RELAY_SERVER_CONFIG_H

#include "common.h"
#include "dir_server_config.h"
typedef struct
{
    server_config_metadata_t dir_conf;
    server_config_metadata_t relay_conf;
}relay_config_metadata_t;

/**
 * @brief Parse command line arguments for relay server configuration
 * 
 * @param argc 
 * @param argv 
 * @param relay_config a pointer to relay_config_metadata_t pointer to store the parsed configuration
 * @return true 
 * @return false 
 */
bool parse_args(const int argc, const char** argv, relay_config_metadata_t** relay_config);


#endif