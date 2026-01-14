#include "create_circuit.h"

static relay_decript_t* get_list_from_dir_server(int sock_fd)
{
    relay_decript_t *relay_list = calloc(MAX_RELAY_BATCH_SIZE, sizeof(relay_decript_t));
    if(relay_list != NULL)
    {
        read_exact(sock_fd, relay_list, MAX_RELAY_BATCH_SIZE * sizeof(relay_decript_t));
    }
    return relay_list;
}

bool connect_to_dir_server(const char *dir_cfg)
{
    bool retval = true;
    server_config_metadata_t *cfg = fetch_server_config((char*)dir_cfg);
    if(!cfg)
    {
        printf("bad config\n");
        retval = false;
    }
    else
    {
        int sock_fd = connect_server(cfg);
        if(sock_fd < 0)
        {
            printf("failed to connect\n");
            retval = false;
        }
        else
        {
            request_t request_to_dir;
            request_to_dir.request_type = CLIENT_REQUEST;
            request_to_dir.request_u.create_circuit_req.exit_index = 0;
            request_to_dir.request_u.create_circuit_req.middle_index = 0;
            request_to_dir.request_u.create_circuit_req.guard_index = 0;
            if(write_exact(sock_fd, &request_to_dir, sizeof(request_t)) == false)
            {
                printf("failed to send request\n");
                retval = false;
            }
            else
            {
                relay_decript_t* relay_list = get_list_from_dir_server(sock_fd);
                int index = 0;
                while ((relay_list + index) != NULL)
                {
                    printf("Relay ID: %u, IP Type: %u, Port: %u, Active: %u\n", (relay_list + index)->relay_id,
                           (relay_list + index)->ip_type,
                           (relay_list + index)->relay_port,
                           (relay_list + index)->is_active);
                }
                
            }

        }
    }
    return retval;
}