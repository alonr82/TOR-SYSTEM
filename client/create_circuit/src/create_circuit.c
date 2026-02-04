#include "create_circuit.h"


static bool create_circuit(relay_decript_t* relays_list, uint32_t relay_list_len);
static void fill_circuit_relays(circuit_t *circuit, relay_decript_t *relays_list);

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
                circuit_t circuit;
                memset(&circuit, 0, sizeof(circuit_t));
                fill_circuit_relays(&circuit, relay_list);
                circuit.guard_fd = circuit_connect_guard(&circuit);
                if(circuit.guard_fd < 0)
                {
                    printf("connect_to_dir_server: circuit_connect_guard failed\n");
                    retval = false;
                }
                else
                {
                    circuit.len = CIRCUIT_LEN;
                    circuit.hops_created = 0;
                    if (build_default_circuit(&circuit, relay_list))
                    {
                        printf("create_circuit succeeded\n");
                        tor_msg_t data;
                        memset(&data, 0, sizeof(data));
                        data.header.type = TOR_MSG_DATA;
                        const char *txt = "This is a test message through the circuit";
                        data.header.payload_len = htons((uint16_t)strlen(txt));
                        memcpy(data.payload, txt, strlen(txt));

                        if (!tor_send_msg(circuit.guard_fd, &data))
                        {
                            printf("[client] send DATA failed\n");
                        }
                        else
                        {
                            printf("[client] sent DATA\n");
                        }
                    }
                    else
                    {
                        printf("create_circuit failed\n");
                        retval = false;
                    }
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

int circuit_connect_guard(circuit_t * circuit)
{
    int guard_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (guard_fd < 0)
    {
        perror("socket");
    }
    else
    {
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(circuit->relay_descripts[0].relay_port);          
        addr.sin_addr.s_addr = ipv4_bytes_to_nbo(circuit->relay_descripts[0].relay_ip); 
        if (connect(guard_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0)
        {
            perror("connect");
            close(guard_fd);
        }
        else
        {
            circuit->guard_fd = guard_fd;
            circuit->hops_created++;
        }
    }
    return guard_fd;
}

bool circuit_extend(circuit_t *circuit)
{
    bool retval = true;
    if (circuit->hops_created >= circuit->len)
    {
        printf("circuit_extend: all hops already created\n");
        retval = false;
    }
    else
    {
        tor_msg_t message;
        memset(&message, 0, sizeof(message));
        message.header.type = TOR_MSG_EXTEND;
        message.header.payload_len = htons(sizeof(tor_extend_t)); 

        tor_extend_t ext;
        memset(&ext, 0, sizeof(ext));
        ext.ip_v4 = ipv4_bytes_to_nbo(circuit->relay_descripts[circuit->hops_created].relay_ip);      
        ext.port  = htons(circuit->relay_descripts[circuit->hops_created].relay_port);             

        memcpy(message.payload, &ext, sizeof(ext));

        if (!tor_send_msg(circuit->guard_fd, &message)) 
        {
            printf("circuit_extend: tor_send_msg EXTEND failed\n");
            retval = false;
        }
        else
        {
            extend_ack_e ack;
            memset(&ack, 0, sizeof(ack));
            if(read_exact(circuit->guard_fd, &ack, sizeof(extend_ack_e)) == false)
            {
                printf("circuit_extend: read_exact EXTEND_ACK failed\n");
                retval = false;
            }
            else
            {
                circuit->hops_created++;
            }
        }
    }
    return retval;
}

static void fill_circuit_relays(circuit_t *circuit, relay_decript_t *relays_list)
{
    for(int index = 0; index < CIRCUIT_LEN; index++)
    {
        circuit->relay_descripts[index] = relays_list[index];
    }
}

bool build_default_circuit(circuit_t *circuit, relay_decript_t *relay_list)
{
    bool retval = true;
    circuit->len = CIRCUIT_LEN;
    circuit->hops_created = 0;
    if(!circuit_connect_guard(circuit))
    {
        printf("build_default_circuit: circuit_connect_guard failed\n");
        retval = false;
    }
    else
    {
        while(circuit->hops_created < circuit->len)
        {
            if(!circuit_extend(circuit))
            {
                printf("build_default_circuit: circuit_extend failed\n");
                retval = false;
                break;
            }
        }
    }
    return retval;
}
