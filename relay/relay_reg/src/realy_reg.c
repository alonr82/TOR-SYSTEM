#include "relay_reg.h"


relay_code_e relay_connect_only(const char* dir_cfg_path)
{
    relay_code_e retval = relay_success;
    server_config_metadata_t* cfg = fetch_server_config((char*)dir_cfg_path);
    if (!cfg)
    {
        printf("relay: bad config\n");
        retval = relay_error;
    }
    else
    {
        int sock_fd = connect_server(cfg);
        if (sock_fd < 0)
        {
            printf("relay: connect FAIL\n");
            retval = relay_error;
        }
        else
        {
            printf("relay: connect SUCCESS (fd=%d)\n", sock_fd);
            sleep(10);
            close(sock_fd);
        }
    }
    return retval;
}