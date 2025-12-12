#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include "relay_server_config.h"

int main(int argc, char **argv)
{
    bool ok = true;
    relay_config_metadata_t *cfg = NULL;

    ok = parse_args(argc, (const char **)argv, &cfg);
    if (!ok)
    {
        printf("parse_args failed\n");
    }
    else
    {
        printf("Relay bind: %u.%u.%u.%u:%u\n",
               cfg->relay_conf.ip_server[0],
               cfg->relay_conf.ip_server[1],
               cfg->relay_conf.ip_server[2],
               cfg->relay_conf.ip_server[3],
               cfg->relay_conf.port_server);

        printf("Directory : %u.%u.%u.%u:%u\n",
               cfg->dir_conf.ip_server[0],
               cfg->dir_conf.ip_server[1],
               cfg->dir_conf.ip_server[2],
               cfg->dir_conf.ip_server[3],
               cfg->dir_conf.port_server);

        free(cfg);
        cfg = NULL;
    }

    return ok ? 0 : 1;
}
