#include "create_circuit.h"

static relay_decript_t* get_list_from_dir_server(int sock_fd, uint32_t *relays_amount)
{
    relay_decript_t *relay_list = calloc(MAX_RELAY_BATCH_SIZE, sizeof(relay_decript_t));
    if(relay_list != NULL)
    {
        read_exact(sock_fd, relays_amount, sizeof(uint32_t));
        read_exact(sock_fd, relay_list, (*relays_amount) * sizeof(relay_decript_t));
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
                uint32_t relays_amount = 0;
                relay_decript_t* relay_list = get_list_from_dir_server(sock_fd, &relays_amount);
                uint32_t index = 0;
                while ( index < relays_amount)
                {
                    printf("Relay ID: %u, IP Type: %u, Port: %u, Active: %u\n", (relay_list + index)->relay_id,
                           (relay_list + index)->ip_type,
                           (relay_list + index)->relay_port,
                           (relay_list + index)->is_active);
                    printf("IP: %u.%u.%u.%u\n", (relay_list + index)->relay_ip[0],
                           (relay_list + index)->relay_ip[1],
                           (relay_list + index)->relay_ip[2],
                           (relay_list + index)->relay_ip[3]);
                    index++;
                }
                free(relay_list);
                
            }

        }
    }
    free(cfg);
    return retval;
}

static bool create_circuit(relay_decript_t* relays_list, uint32_t relay_list_len)
{
    bool retval = true;
    tor_msg_t message;
    message.header.type = TOR_MSG_EXTEND;
    tor_extend_t extended_relay_data;
    extended_relay_data.ip_v4 = relays_list[1].relay_ip;
    extended_relay_data.port = relays_list[1].relay_port;
    memcpy(message.payload, &extended_relay_data, sizeof(extended_relay_data));
    int guard_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(guard_fd >= 0)
    {
        struct sockaddr_in addr;
        memset(&addr,0,sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = relays_list[0].relay_port;
        addr.sin_addr.s_addr = relays_list[0].relay_ip;
        int connect_result = connect(guard_fd,(struct sockaddr*)&addr, sizeof(addr));
        if(connect_result >= 0)
        {
            write_exact(guard_fd,&message,sizeof(message));
        }
    }
}