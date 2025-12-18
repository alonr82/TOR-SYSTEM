#ifndef RELAY_REG_H
#define RELAY_REG_H

#include "sock_utilities.h"
#include "common/common.h"

typedef enum
{
    relay_success = 0,
    relay_error
}relay_code_e;

/**
 * @brief this function connects to the directory server based on the given configuration file
 * 
 */
relay_code_e relay_connect_only(const char* dir_cfg_path);

#endif