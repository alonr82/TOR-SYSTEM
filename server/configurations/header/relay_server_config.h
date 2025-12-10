#ifndef RELAY_SERVER_CONFIG_H
#define RELAY_SERVER_CONFIG_H

#include "../../common/common.h"
#include "dir_server_config.h"
typedef struct
{
    server_config_metadata_t dir_conf;
    server_config_metadata_t relay_conf;
}relay_config_metadata_t;




#endif