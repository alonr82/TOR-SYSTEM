#include "create_circuit.h"
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

static const uint8_t DIR_SERVER_PUB_KEY[32] = {
    0xd7, 0x5a, 0x98, 0x01, 0x82, 0xb1, 0x0a, 0xb7, 
    0xd5, 0x4b, 0xfe, 0xd3, 0xc9, 0x64, 0x07, 0x3a, 
    0x0e, 0xe1, 0x72, 0xf3, 0xda, 0xa6, 0x23, 0x25, 
    0xaf, 0x02, 0x1a, 0x68, 0xf7, 0x07, 0x51, 0x1a
};

static void fill_circuit_relays(circuit_t *circuit, relay_decript_t *relays_list)
{
    for(int index = 0; index < CIRCUIT_LEN; index++)
    {
        circuit->relay_descripts[index] = relays_list[index];
    }
}

static relay_decript_t* get_list_from_dir_server(int sock_fd, uint32_t *relays_amount)
{
    relay_decript_t *relay_list = calloc(MAX_RELAY_BATCH_SIZE, sizeof(relay_decript_t));
    if(relay_list != NULL)
    {
        if(read_exact(sock_fd, relays_amount, sizeof(uint32_t)) == false) {
            free(relay_list); return NULL;
        }
        uint8_t signature[64];
        if(read_exact(sock_fd, signature, 64) == false) {
            free(relay_list); return NULL;
        }
        if(read_exact(sock_fd, relay_list, (*relays_amount) * sizeof(relay_decript_t)) == false) {
            free(relay_list); return NULL;
        }
        if (!tor_ed25519_verify(DIR_SERVER_PUB_KEY, signature, (const uint8_t*)relay_list, (*relays_amount) * sizeof(relay_decript_t)))
        {
            printf("\n[!!! SECURITY ALERT !!!] Directory Server Signature INVALID!\n");
            printf("Potential Man-in-the-Middle (MITM) attack detected. Dropping connection.\n> ");
            fflush(stdout);
            free(relay_list);
            return NULL;
        }
        
        printf("[Security] Directory Server identity and relay list verified successfully.\n");
    }
    return relay_list;
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
        printf("circuit_connect_guard: attempting to connect to IP: %u.%u.%u.%u, Port: %u\n",
                circuit->relay_descripts[0].relay_ip[0],
                circuit->relay_descripts[0].relay_ip[1],
                circuit->relay_descripts[0].relay_ip[2],
                circuit->relay_descripts[0].relay_ip[3],
                circuit->relay_descripts[0].relay_port);

        struct sockaddr_in addr;
        memset(&addr,0,sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(circuit->relay_descripts[0].relay_port);
        addr.sin_addr.s_addr = ipv4_bytes_to_nbo(circuit->relay_descripts[0].relay_ip);

        if (connect(guard_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0)
        {
            perror("circuit_connect_guard: connect failed");
            close(guard_fd);
            guard_fd = -1;
        }
        else
        {
            printf("circuit_connect_guard: successfully connected to guard relay!\n");
            circuit->guard_fd = guard_fd;
        }
    }
    return guard_fd;
}

bool send_onion_data(circuit_t *circuit, const uint8_t *plaintext, uint16_t plaintext_len)
{
    bool retval = true;
    uint8_t current_buffer[TOR_MSG_SIZE];
    uint16_t current_len = plaintext_len;

    memcpy(current_buffer, plaintext, plaintext_len);

    for (int i = (int)circuit->hops_created - 1; i >= 0; i--)
    {
        uint8_t encrypted_blob[TOR_MSG_SIZE];
        uint16_t encrypted_blob_len = 0;

        if (!gen_encrypted_framed_message(current_buffer, current_len, circuit->hop_keys[i], encrypted_blob, &encrypted_blob_len))
        {
            printf("send_onion_data: failed to encrypt layer %d\n", i);
            retval = false;
            break;
        }

        tor_msg_t wrapped_msg;
        memset(&wrapped_msg, 0, sizeof(wrapped_msg));
        wrapped_msg.header.type = TOR_MSG_DATA;
        wrapped_msg.header.payload_len = htons(encrypted_blob_len);
        memcpy(wrapped_msg.payload, encrypted_blob, encrypted_blob_len);

        current_len = (uint16_t)(sizeof(tor_header_t) + encrypted_blob_len);
        memcpy(current_buffer, &wrapped_msg, current_len);
    }

    if (retval)
    {
        if (!tor_send_msg(circuit->guard_fd, (tor_msg_t*)current_buffer))
        {
            printf("send_onion_data: failed to send to guard relay\n");
            retval = false;
        }
    }

    return retval;
}

bool recv_onion_data(circuit_t *circuit, tor_msg_t *in_msg, uint8_t *out_plaintext, uint16_t *out_len)
{
    bool retval = true;
    if(in_msg->header.type != TOR_MSG_DATA)
    {
        printf("recv_onion_data: outermost message is NOT DATA (type: %d)\n", in_msg->header.type);
        retval = false;
    }
    else
    {
        uint8_t current_payload[TOR_MSG_SIZE];
        uint16_t current_payload_len = ntohs(in_msg->header.payload_len);
        memcpy(current_payload, in_msg->payload, current_payload_len);

        uint8_t decrypted_data[TOR_MSG_SIZE];
        uint16_t decrypted_len = 0;

        printf("\n--- Start Peeling Onion (%d layers) ---\n", circuit->hops_created);

        for(uint32_t i = 0; i < circuit->hops_created; i++)
        {
            printf("recv_onion_data: Peeling layer %d...\n", i);
            if(!decrypt_blob(current_payload, &current_payload_len, circuit->hop_keys[i], decrypted_data, &decrypted_len))
            {
                printf("recv_onion_data: decrypt_blob failed at hop %d!\n", i);
                retval = false;
                break;
            }

            if (i < circuit->hops_created - 1)
            {
                tor_msg_t *peeled_msg = (tor_msg_t*)decrypted_data;
                if (peeled_msg->header.type != TOR_MSG_DATA)
                {
                    printf("recv_onion_data: expected inner TOR_MSG_DATA at layer %d, got %d\n", i, peeled_msg->header.type);
                    retval = false;
                    break;
                }
                current_payload_len = ntohs(peeled_msg->header.payload_len);
                memcpy(current_payload, peeled_msg->payload, current_payload_len);
            }
            else
            {
                tor_msg_t *final_msg = (tor_msg_t*)decrypted_data;
                printf("recv_onion_data: Reached core! Final message type: %d\n", final_msg->header.type);
                memcpy(out_plaintext, decrypted_data, decrypted_len);
                *out_len = decrypted_len;
            }
        }
        printf("--- Finished Peeling ---\n\n");
    }
    return retval;
}

static bool derive_and_save_keys(circuit_t *circuit, const uint8_t client_ephemeral_priv[TOR_X25519_KEY_LEN], const uint8_t relay_pub[TOR_X25519_KEY_LEN])
{
    bool retval = true;
    uint8_t shared_secret[TOR_SHARED_SECRET_LEN];

    if(!derive_shared_secret(shared_secret, client_ephemeral_priv, relay_pub))
    {
        printf("derive_and_save_keys: derive_shared_secret failed\n");
        retval = false;
    }
    else
    {
        uint8_t salt[1] = {0};
        if(!hkdf_derive_key(circuit->hop_keys[circuit->hops_created], E2E_KEY_LEN, shared_secret, TOR_SHARED_SECRET_LEN, salt, 0, (const uint8_t*)"TOR_HOP_KEY", 11))
        {
            printf("derive_and_save_keys: hkdf_derive_key failed\n");
            retval = false;
        }
        else
        {
            circuit->hops_created++;
        }
    }
    return retval;
}

static bool send_create_msg(int fd, const uint8_t pub_key[TOR_X25519_KEY_LEN])
{
    bool retval = true;
    tor_msg_t msg;
    memset(&msg, 0, sizeof(msg));
    msg.header.type = TOR_MSG_CREATE;

    tor_create_t *create = (tor_create_t*)msg.payload;
    memcpy(create->client_x25519_pub, pub_key, TOR_X25519_KEY_LEN);

    msg.header.payload_len = htons(sizeof(tor_create_t));

    if(!tor_send_msg(fd, &msg))
    {
        printf("send_create_msg: failed to send CREATE\n");
        retval = false;
    }
    return retval;
}

bool circuit_create(circuit_t *circuit)
{
    bool retval = true;
    uint8_t client_ephemeral_pub[TOR_X25519_KEY_LEN];
    uint8_t client_ephemeral_priv[TOR_X25519_KEY_LEN];

    if(!generate_empherial_keypair(client_ephemeral_pub, client_ephemeral_priv))
    {
        printf("circuit_create: failed to generate keys\n");
        retval = false;
    }
    else
    {
        if(!send_create_msg(circuit->guard_fd, client_ephemeral_pub))
        {
            retval = false;
        }
        else
        {
            tor_msg_t res;
            memset(&res, 0, sizeof(res));

            if(!tor_recv_msg(circuit->guard_fd, &res))
            {
                printf("circuit_create: tor_recv_msg failed\n");
                retval = false;
            }
            else
            {
                if(res.header.type != TOR_MSG_CREATED)
                {
                    printf("circuit_create: wrong type!\n");
                    retval = false;
                }
                else
                {
                    tor_created_t *created = (tor_created_t*)res.payload;

                    uint8_t *relay_id_pub = circuit->relay_descripts[0].identify_pub;
                    if(!tor_ed25519_verify(relay_id_pub, created->sig, created->relay_x25519_pub, TOR_X25519_KEY_LEN))
                    {
                        printf("circuit_create: Signature verification FAILED\n");
                        retval = false;
                    }
                    else
                    {
                        if(!derive_and_save_keys(circuit, client_ephemeral_priv, created->relay_x25519_pub))
                        {
                            retval = false;
                        }
                        else
                        {
                            printf("circuit_create: handshake with Guard succeeded!\n");
                        }
                    }
                }
            }
        }
    }
    return retval;
}

static bool send_extend_request(circuit_t *circuit, uint8_t client_ephemeral_pub[TOR_X25519_KEY_LEN])
{
    bool retval = true;
    tor_msg_t message;
    memset(&message, 0, sizeof(message));
    message.header.type = TOR_MSG_EXTEND;

    tor_extend_t ext;
    memset(&ext, 0, sizeof(ext));

    ext.ip_v4 = ipv4_bytes_to_nbo(circuit->relay_descripts[circuit->hops_created].relay_ip);
    ext.port  = htons(circuit->relay_descripts[circuit->hops_created].relay_port);
    memcpy(ext.create_data.client_x25519_pub, client_ephemeral_pub, TOR_X25519_KEY_LEN);

    message.header.payload_len = htons(sizeof(tor_extend_t));
    memcpy(message.payload, &ext, sizeof(ext));

    if (!send_onion_data(circuit, (uint8_t*)&message, (uint16_t)(sizeof(tor_header_t) + sizeof(tor_extend_t))))
    {
        printf("send_extend_request: send_onion_data failed\n");
        retval = false;
    }
    return retval;
}

static bool verify_extend_response(circuit_t *circuit, uint8_t client_ephemeral_priv[TOR_X25519_KEY_LEN])
{
    bool retval = true;

    tor_msg_t res;
    memset(&res, 0, sizeof(res));

    if(!tor_recv_msg(circuit->guard_fd, &res))
    {
        printf("verify_extend_response: tor_recv_msg failed\n");
        retval = false;
    }
    else
    {
        uint8_t plaintext[TOR_MSG_SIZE];
        uint16_t plaintext_len = 0;

        if(!recv_onion_data(circuit, &res, plaintext, &plaintext_len))
        {
            retval = false;
        }
        else
        {
            tor_msg_t *decrypted_msg = (tor_msg_t*)plaintext;

            if(decrypted_msg->header.type != TOR_MSG_EXTENDED)
            {
                retval = false;
            }
            else
            {
                tor_extended_t *extended = (tor_extended_t*)decrypted_msg->payload;
                if(extended->status != ACK_EXTEND_OK)
                {
                    retval = false;
                }
                else
                {
                    extern bool g_client_defense_active;
                    uint8_t *relay_id_pub = circuit->relay_descripts[circuit->hops_created].identify_pub;
                    
                    usleep(3000000); 

                    if(!tor_ed25519_verify(relay_id_pub, extended->created_data.sig, extended->created_data.relay_x25519_pub, TOR_X25519_KEY_LEN))
                    {
                        if(g_client_defense_active == true)
                        {
                            printf("\n [defense is active] invalid signature in EXTENDED!\n");
                            fflush(stdout);
                            retval = false;
                        }
                        else
                        {
                            printf("[!!! SECURITY ALERT !!!] Invalid signature in EXTENDED response!\n");
                            printf("defense against malicious relay is NOT active.\n");
                            fflush(stdout);
                        }
                    }
                    else
                    {
                        if(!derive_and_save_keys(circuit, client_ephemeral_priv, extended->created_data.relay_x25519_pub))
                        {
                            retval = false;
                        }
                    }
                }
            }
        }
    }

    return retval;
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
        uint8_t client_ephemeral_pub[TOR_X25519_KEY_LEN];
        uint8_t client_ephemeral_priv[TOR_X25519_KEY_LEN];

        if(!generate_empherial_keypair(client_ephemeral_pub, client_ephemeral_priv))
        {
            printf("circuit_extend: failed to generate keys\n");
            retval = false;
        }
        else
        {
            if(!send_extend_request(circuit, client_ephemeral_pub))
            {
                retval = false;
            }
            else
            {
                if(!verify_extend_response(circuit, client_ephemeral_priv))
                {
                    retval = false;
                }
            }
        }
    }
    return retval;
}

bool build_default_circuit(circuit_t *circuit, relay_decript_t *relay_list)
{
    bool retval = true;
    circuit->len = CIRCUIT_LEN;
    circuit->hops_created = 0;

    printf("Selecting route...\n");
    fflush(stdout);
    usleep(500000);

    if(circuit_connect_guard(circuit) < 0)
    {
        printf("build_default_circuit: circuit_connect_guard failed\n");
        retval = false;
    }
    else
    {
        if(!circuit_create(circuit))
        {
            printf("build_default_circuit: circuit_create failed\n");
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
    }
    (void)relay_list;
    return retval;
}

static bool send_routed_inner(circuit_t *circuit,
                             uint32_t peer_ip_v4_nbo,
                             uint16_t peer_port_nbo,
                             const uint8_t *inner_bytes,
                             uint16_t inner_len)
{
    bool retval = true;

    tor_msg_t routed;
    memset(&routed, 0, sizeof(routed));
    routed.header.type = TOR_MSG_DATA;

    tor_data_route_t *route = (tor_data_route_t*)routed.payload;
    route->dest_ip_v4 = peer_ip_v4_nbo;
    route->dest_port  = peer_port_nbo;
    route->inner_len  = htons(inner_len);
    memcpy(route->inner, inner_bytes, inner_len);

    uint16_t route_hdr_len = (uint16_t)(sizeof(uint32_t) + sizeof(uint16_t) + sizeof(uint16_t));
    uint16_t routed_payload_len = (uint16_t)(route_hdr_len + inner_len);

    routed.header.payload_len = htons(routed_payload_len);

    if(!send_onion_data(circuit, (const uint8_t*)&routed, (uint16_t)(sizeof(tor_header_t) + routed_payload_len)))
    {
        retval = false;
    }

    return retval;
}

bool chat_send_keyx(circuit_t *circuit,
                    uint32_t peer_ip_v4_nbo,
                    uint16_t peer_port_nbo,
                    const uint8_t key[E2E_KEY_LEN],
                    const uint8_t my_identity[32])
{
    bool retval = true;

    tor_msg_t inner;
    memset(&inner, 0, sizeof(inner));
    inner.header.type = TOR_MSG_DATA;

    uint8_t *p = inner.payload;
    memcpy(p, "KEYX", 4);
    memcpy(p + 4, key, E2E_KEY_LEN);
    memcpy(p + 4 + E2E_KEY_LEN, my_identity, 32);

    uint16_t payload_len = 4 + E2E_KEY_LEN + 32;
    inner.header.payload_len = htons(payload_len);

    uint16_t inner_len = (uint16_t)(sizeof(tor_header_t) + payload_len);

    if(!send_routed_inner(circuit, peer_ip_v4_nbo, peer_port_nbo, (const uint8_t*)&inner, inner_len))
    {
        retval = false;
    }

    return retval;
}

bool chat_send_encrypted(circuit_t *circuit,
                         uint32_t peer_ip_v4_nbo,
                         uint16_t peer_port_nbo,
                         const uint8_t e2e_key[E2E_KEY_LEN],
                         const uint8_t *plaintext,
                         uint16_t plaintext_len)
{
    bool retval = true;

    uint8_t cipher[TOR_MSG_SIZE];
    uint16_t cipher_len = 0;

    if(!gen_encrypted_framed_message(plaintext, plaintext_len, e2e_key, cipher, &cipher_len))
    {
        retval = false;
    }
    else
    {
        tor_msg_t inner;
        memset(&inner, 0, sizeof(inner));
        inner.header.type = TOR_MSG_DATA;
        inner.header.payload_len = htons(cipher_len);
        memcpy(inner.payload, cipher, cipher_len);

        uint16_t inner_len = (uint16_t)(sizeof(tor_header_t) + cipher_len);

        if(!send_routed_inner(circuit, peer_ip_v4_nbo, peer_port_nbo, (const uint8_t*)&inner, inner_len))
        {
            retval = false;
        }
    }

    return retval;
}

relay_decript_t* fetch_relays_from_dir(const char *dir_cfg, uint32_t *out_amount)
{
    relay_decript_t *relay_list = NULL;
    server_config_metadata_t *cfg = fetch_server_config((char*)dir_cfg);
    
    if(cfg != NULL)
    {
        int sock_fd = connect_server(cfg);
        if(sock_fd >= 0)
        {
            request_t request_to_dir;
            memset(&request_to_dir, 0, sizeof(request_t));
            request_to_dir.request_type = CLIENT_REQUEST;

            if(write_exact(sock_fd, &request_to_dir, sizeof(request_t)) == true)
            {
                relay_list = get_list_from_dir_server(sock_fd, out_amount);
            }
            close(sock_fd);
        }
        free(cfg);
    }
    return relay_list;
}

bool build_circuit_with_relays(circuit_t *out_circuit, relay_decript_t *chosen_relays)
{
    memset(out_circuit, 0, sizeof(*out_circuit));
    fill_circuit_relays(out_circuit, chosen_relays);
    return build_default_circuit(out_circuit, chosen_relays);
}

void notify_directory_of_route(const char *dir_cfg, uint32_t id1, uint32_t id2, uint32_t id3)
{
    server_config_metadata_t *cfg = fetch_server_config((char*)dir_cfg);
    if(cfg != NULL)
    {
        int sock_fd = connect_server(cfg);
        if(sock_fd >= 0)
        {
            request_t req;
            memset(&req, 0, sizeof(request_t));
            req.request_type = CLIENT_ROUTE_SELECTED;
            req.request_u.create_circuit_req.guard_index = id1;
            req.request_u.create_circuit_req.middle_index = id2;
            req.request_u.create_circuit_req.exit_index = id3;
            write_exact(sock_fd, &req, sizeof(request_t));
            close(sock_fd);
        }
        free(cfg);
    }
}

bool connect_to_dir_server(const char *dir_cfg)
{
    circuit_t c;
    uint32_t amount = 0;
    relay_decript_t* relays = fetch_relays_from_dir(dir_cfg, &amount);
    if (!relays) return false;
    bool res = build_circuit_with_relays(&c, relays);
    free(relays);
    return res;
}