#ifndef DIR_SERVER_CONFIG_H
#define DIR_SERVER_CONFIG_H

#include "common.h"
#include <string.h>

#define MAX_FILE_LEN 256
#define IP_TITLE_LEN 3
#define PORT_TITLE_LEN 5

typedef struct 
{
    uint8_t ip_server[IP4_SIZE];
    uint16_t port_server;
}server_config_metadata_t;

/*TODO: fix the check for valid values for IP + PORT*/

/**
 * @brief this function fetches the ip from a line we read in a configuration file
 * 
 * @param server_config the pointer to our struct we fill
 * @param line the line we read from the file
 * @return true 
 * @return false 
 */
bool fetch_server_ip_config(server_config_metadata_t *server_config, char* line);

/**
 * @brief this function fetches the port from a line we read in a configuration file
 * 
 * @param server_config the pointer to our struct we fill
 * @param line the line we read from the file
 * @return true 
 * @return false 
 */
bool fetch_server_port_config(server_config_metadata_t *server_config, char *line);


/**
 * @brief this function fetches the server configuration from a given file path
 * @param filepath the path to the configuration file
 * @return server_config_metadata_t* pointer to the server configuration structure
 * @note the configuration file must be in the following format:
 * ip=x.x.x.x
 * port=number
 * 
 */
server_config_metadata_t* fetch_server_config(char* filepath);









#endif