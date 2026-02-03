#include "create_circuit.h"
static bool create_circuit(relay_decript_t* relays_list, uint32_t relay_list_len);

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
                if (!create_circuit(relay_list, relays_amount))
                {
                    printf("create_circuit failed\n");
                    retval = false;
                }
                free(relay_list);
            }
        }
        close(sock_fd);
    }
    free(cfg);
    return retval;
}

static uint32_t ipv4_bytes_to_nbo(const uint8_t ip[4])
{
    uint32_t retval =
        ((uint32_t)ip[0] << 24) |
        ((uint32_t)ip[1] << 16) |
        ((uint32_t)ip[2] << 8)  |
        ((uint32_t)ip[3]);
    return htonl(retval);
}


static bool create_circuit(relay_decript_t* relays_list, uint32_t relay_list_len)
{
    if (!relays_list || relay_list_len < 2)
    {
        printf("create_circuit: need at least 2 relays\n");
        return false;
    }
    tor_msg_t message;
    memset(&message, 0, sizeof(message));
    message.header.type = TOR_MSG_EXTEND;
    message.header.payload_len = htons(sizeof(tor_extend_t)); 

    tor_extend_t ext;
    memset(&ext, 0, sizeof(ext));
    ext.ip_v4 = ipv4_bytes_to_nbo(relays_list[1].relay_ip);      
    ext.port  = htons(relays_list[1].relay_port);             

    memcpy(message.payload, &ext, sizeof(ext));
    int guard_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (guard_fd < 0)
    {
        perror("socket");
        return false;
    }
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(relays_list[0].relay_port);          
    addr.sin_addr.s_addr = ipv4_bytes_to_nbo(relays_list[0].relay_ip); 
    if (connect(guard_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0)
    {
        perror("connect");
        close(guard_fd);
        return false;
    }

    if (!tor_send_msg(guard_fd, &message)) 
    {
        printf("create_circuit: tor_send_msg EXTEND failed\n");
        close(guard_fd);
        return false;
    }
    tor_msg_t data;
    memset(&data, 0, sizeof(data));
    data.header.type = TOR_MSG_DATA;

    const char *txt = "hi";
    data.header.payload_len = htons((uint16_t)strlen(txt));
    memcpy(data.payload, txt, strlen(txt));

    if (!tor_send_msg(guard_fd, &data))
    {
        printf("create_circuit: tor_send_msg DATA failed\n");
        close(guard_fd);
        return false;
    }

    close(guard_fd);
    return true;
}
