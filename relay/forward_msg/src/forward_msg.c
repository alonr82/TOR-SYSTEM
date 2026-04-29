#include "forward_msg.h"

#include <stdio.h>
#include <arpa/inet.h>

static void log_attacker_relay_message(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    printf("[ATTACKER RELAY] ");
    vprintf(format, args);
    printf("\n");
    fflush(stdout);
    va_end(args);
}

static void log_attacker_packet_metadata(const char* direction, const tor_msg_t *message)
{
    if(g_relay_runtime_mode == relay_runtime_mode_attacker_controlled)
    {
        if(message != NULL)
        {
            log_attacker_relay_message("direction=%s outer_type=%u payload_len=%u",
                                       direction,
                                       message->header.type,
                                       ntohs(message->header.payload_len));
        }
    }
}

static void log_attacker_extend_metadata(const tor_extend_t *ext)
{
    if(g_relay_runtime_mode == relay_runtime_mode_attacker_controlled)
    {
        if(ext != NULL)
        {
            uint32_t ip_nbo = ext->ip_v4;
            struct in_addr addr;
            char ip_str[INET_ADDRSTRLEN];

            addr.s_addr = ip_nbo;
            memset(ip_str, 0, sizeof(ip_str));
            inet_ntop(AF_INET, &addr, ip_str, sizeof(ip_str));

            log_attacker_relay_message("observed EXTEND target=%s:%u",
                                       ip_str,
                                       ntohs(ext->port));
        }
    }
}

static void log_attacker_exit_metadata(const tor_data_route_t *route)
{
    if(g_relay_runtime_mode == relay_runtime_mode_attacker_controlled)
    {
        if(route != NULL)
        {
            struct in_addr addr;
            char ip_str[INET_ADDRSTRLEN];
            uint16_t inner_len = ntohs(route->inner_len);

            addr.s_addr = route->dest_ip_v4;
            memset(ip_str, 0, sizeof(ip_str));
            inet_ntop(AF_INET, &addr, ip_str, sizeof(ip_str));

            log_attacker_relay_message("observed EXIT routing destination=%s:%u inner_len=%u",
                                       ip_str,
                                       ntohs(route->dest_port),
                                       inner_len);
            log_attacker_relay_message("payload remains end-to-end encrypted, content not visible");
        }
    }
}

bool process_create_handshake(session_t *session, tor_msg_t *in_msg, tor_msg_t *out_msg, const uint8_t *priv_seed)
{
    bool retval = true;
    tor_create_t *create_req = (tor_create_t *)in_msg->payload;
    tor_created_t *created_res = (tor_created_t *)out_msg->payload;

    uint8_t relay_ephemeral_pub[TOR_X25519_KEY_LEN];
    uint8_t relay_ephemeral_priv[TOR_X25519_KEY_LEN];
    uint8_t shared_secret[TOR_SHARED_SECRET_LEN];

    if(!generate_empherial_keypair(relay_ephemeral_pub, relay_ephemeral_priv))
    {
        retval = false;
    }
    else
    {
        if(!derive_shared_secret(shared_secret, relay_ephemeral_priv, create_req->client_x25519_pub))
        {
            retval = false;
        }
        else
        {
            uint8_t salt[1] = {0};
            if(!hkdf_derive_key(session->hop_key, E2E_KEY_LEN, shared_secret, TOR_SHARED_SECRET_LEN, salt, 0, (const uint8_t*)"TOR_HOP_KEY", 11))
            {
                retval = false;
            }
            else
            {
                session->hop_key_ready = true;
                if(!tor_ed25519_sign(created_res->sig, priv_seed, relay_ephemeral_pub, TOR_X25519_KEY_LEN))
                {
                    retval = false;
                }
                else
                {
                    memcpy(created_res->relay_x25519_pub, relay_ephemeral_pub, TOR_X25519_KEY_LEN);
                    out_msg->header.type = TOR_MSG_CREATED;
                    out_msg->header.payload_len = htons(sizeof(tor_created_t));

                    if(g_relay_runtime_mode == relay_runtime_mode_attacker_controlled)
                    {
                        log_attacker_relay_message("completed CREATE handshake with previous hop");
                    }
                }
            }
        }
    }
    return retval;
}

static bool handle_extend_request(session_t *session, tor_msg_t *decrypted_msg, tor_msg_t *reply_msg)
{
    bool retval = true;
    tor_extend_t *ext = (tor_extend_t *)decrypted_msg->payload;

    log_attacker_extend_metadata(ext);
    simulation_state_t sim_state = simulation_state_get_copy();
    if(g_relay_runtime_mode == relay_runtime_mode_attacker_controlled)
    {
        log_attacker_relay_message("simulated attack: Circuit hijacked!\n");
        tor_extended_t *extended_response = (tor_extended_t *)reply_msg->payload;
        extended_response->status = ACK_EXTEND_OK;
        memset(extended_response->created_data.sig, 0xFF, TOR_ED25519_SIG_LEN);
        reply_msg->header.type = TOR_MSG_EXTENDED;
        reply_msg->header.payload_len = htons(sizeof(tor_extended_t));
        log_attacker_relay_message("Returning FAKE extended response to client");
    }
    else
    {
        int extended_fd = socket(AF_INET, SOCK_STREAM, 0);
        if(extended_fd < 0)
        {
            retval = false;
        }
        else
        {
            struct sockaddr_in addr;
            memset(&addr,0,sizeof(addr));
            addr.sin_family = AF_INET;
            addr.sin_port = ext->port;
            addr.sin_addr.s_addr = ext->ip_v4;

            if(connect(extended_fd,(struct sockaddr*)&addr, sizeof(addr)) < 0)
            {
                close(extended_fd);
                retval = false;
            }
            else
            {
                session->next_fd = extended_fd;

                tor_msg_t create_msg;
                memset(&create_msg, 0, sizeof(create_msg));
                create_msg.header.type = TOR_MSG_CREATE;
                create_msg.header.payload_len = htons(sizeof(tor_create_t));
                memcpy(create_msg.payload, &ext->create_data, sizeof(tor_create_t));

                if(!tor_send_msg(session->next_fd, &create_msg))
                {
                    retval = false;
                }
                else
                {
                    tor_msg_t created_msg;
                    if(!tor_recv_msg(session->next_fd, &created_msg) || created_msg.header.type != TOR_MSG_CREATED)
                    {
                        retval = false;
                    }
                    else
                    {
                        tor_extended_t *extended_res = (tor_extended_t *)reply_msg->payload;
                        extended_res->status = ACK_EXTEND_OK;
                        memcpy(&extended_res->created_data, created_msg.payload, sizeof(tor_created_t));

                        reply_msg->header.type = TOR_MSG_EXTENDED;
                        reply_msg->header.payload_len = htons(sizeof(tor_extended_t));

                        if(g_relay_runtime_mode == relay_runtime_mode_attacker_controlled)
                        {
                            log_attacker_relay_message("extended circuit successfully to next hop");
                        }
                    }
                }
            }
        }
    }
    
    return retval;
}

static bool process_extend_in_forward(session_t *session, tor_msg_t *decrypted_msg)
{
    bool retval = true;
    tor_msg_t reply_msg;
    memset(&reply_msg, 0, sizeof(reply_msg));

    if(!handle_extend_request(session, decrypted_msg, &reply_msg))
    {
        retval = false;
    }
    else
    {
        tor_msg_t encrypted_reply;
        memset(&encrypted_reply, 0, sizeof(encrypted_reply));
        encrypted_reply.header.type = TOR_MSG_DATA;
        uint16_t frame_len = 0;

        if(!gen_encrypted_framed_message((uint8_t*)&reply_msg, (uint16_t)(sizeof(tor_header_t) + ntohs(reply_msg.header.payload_len)), session->hop_key, encrypted_reply.payload, &frame_len))
        {
            retval = false;
        }
        else
        {
            encrypted_reply.header.payload_len = htons(frame_len);
            if(!tor_send_msg(session->last_fd, &encrypted_reply))
            {
                retval = false;
            }
        }
    }
    return retval;
}

/*
 * EXIT ONLY:
 * decrypted_msg is tor_msg_t (type DATA) whose payload begins with tor_data_route_t
 * Route: connect to peer dest, then forward inner tor_msg_t bytes.
 */
static bool process_data_in_forward(session_t *session, tor_msg_t *decrypted_msg)
{
    bool retval = true;

    uint16_t payload_len = ntohs(decrypted_msg->header.payload_len);
    if(payload_len < (uint16_t)(sizeof(uint32_t) + sizeof(uint16_t) + sizeof(uint16_t)))
    {
        printf("process_data_in_forward: bad routed DATA size\n");
        retval = false;
    }
    else
    {
        tor_data_route_t *route = (tor_data_route_t*)decrypted_msg->payload;

        log_attacker_exit_metadata(route);

        uint16_t inner_len = ntohs(route->inner_len);
        uint16_t route_hdr_len = (uint16_t)(sizeof(uint32_t) + sizeof(uint16_t) + sizeof(uint16_t));

        if(inner_len < sizeof(tor_header_t) || inner_len > (uint16_t)(payload_len - route_hdr_len))
        {
            printf("process_data_in_forward: inner_len mismatch (inner=%u, payload=%u)\n", inner_len, payload_len);
            retval = false;
        }
        else
        {
            if(!session->dest_ready ||
               session->dest_ip_v4 != route->dest_ip_v4 ||
               session->dest_port  != route->dest_port)
            {
                if(session->next_fd >= 0)
                {
                    close(session->next_fd);
                    session->next_fd = -1;
                }

                int dest_fd = connect_to_peer(route->dest_ip_v4, route->dest_port);
                if(dest_fd < 0)
                {
                    retval = false;
                }
                else
                {
                    session->next_fd = dest_fd;
                    session->dest_ready = true;
                    session->dest_ip_v4 = route->dest_ip_v4;
                    session->dest_port  = route->dest_port;
                }
            }

            if(retval)
            {
                tor_msg_t inner_msg;
                memset(&inner_msg, 0, sizeof(inner_msg));
                memcpy(&inner_msg, route->inner, inner_len);

                if(!tor_send_msg(session->next_fd, &inner_msg))
                {
                    retval = false;
                }
            }
        }
    }
    return retval;
}

static bool route_decrypted_msg(session_t *session, tor_msg_t *decrypted_msg)
{
    bool retval = true;

    if(decrypted_msg->header.type == TOR_MSG_DATA)
    {
        /*
         * Exit behavior:
         * If next_fd == -1 => still not connected to peer, parse route+connect.
         * If dest_ready == true => keep parsing route for each message (same peer) and send inner.
         * Otherwise (middle/guard that already has next_fd and dest_ready==false): forward decrypted_msg to next hop.
         */
        if(session->next_fd == -1 || session->dest_ready)
        {
            if(!process_data_in_forward(session, decrypted_msg))
            {
                retval = false;
            }
        }
        else
        {
            if(!tor_send_msg(session->next_fd, decrypted_msg))
            {
                retval = false;
            }
        }
    }
    else
    {
        if(session->next_fd == -1)
        {
            if(decrypted_msg->header.type == TOR_MSG_EXTEND)
            {
                if(!process_extend_in_forward(session, decrypted_msg))
                {
                    retval = false;
                }
            }
        }
        else
        {
            if(!tor_send_msg(session->next_fd, decrypted_msg))
            {
                retval = false;
            }
        }
    }

    return retval;
}

static bool handle_peeled_data(session_t *session, tor_msg_t *in_msg)
{
    bool retval = true;
    uint16_t blob_len = ntohs(in_msg->header.payload_len);
    uint8_t plaintext[TOR_MSG_SIZE];
    uint16_t plaintext_len = 0;

    if(!decrypt_blob(in_msg->payload, &blob_len, session->hop_key, plaintext, &plaintext_len))
    {
        retval = false;
    }
    else
    {
        tor_msg_t *decrypted_msg = (tor_msg_t *)plaintext;
        if(g_relay_runtime_mode == relay_runtime_mode_attacker_controlled)
        {
            log_attacker_relay_message("peeled one onion layer successfully");
        }
        if(!route_decrypted_msg(session, decrypted_msg))
        {
            retval = false;
        }
    }
    return retval;
}

static bool forward_last_to_next(session_t *session)
{
    bool retval = true;
    tor_msg_t in_msg;
    memset(&in_msg, 0, sizeof(in_msg));

    if(!tor_recv_msg(session->last_fd, &in_msg))
    {
        retval = false;
    }
    else
    {
        log_attacker_packet_metadata("prev_to_this", &in_msg);

        if(in_msg.header.type == TOR_MSG_DATA)
        {
            if(session->hop_key_ready)
            {
                if(!handle_peeled_data(session, &in_msg))
                {
                    retval = false;
                }
            }
            else
            {
                if(session->next_fd != -1)
                {
                    if(!tor_send_msg(session->next_fd, &in_msg))
                    {
                        retval = false;
                    }
                }
            }
        }
        else
        {
            if(session->next_fd != -1)
            {
                if(!tor_send_msg(session->next_fd, &in_msg))
                {
                    retval = false;
                }
            }
        }
    }
    return retval;
}

static bool forward_next_to_last(session_t *session)
{
    bool retval = true;
    tor_msg_t in_msg;
    memset(&in_msg, 0, sizeof(in_msg));

    if(!tor_recv_msg(session->next_fd, &in_msg))
    {
        retval = false;
    }
    else
    {
        log_attacker_packet_metadata("next_to_this", &in_msg);

        if(session->hop_key_ready)
        {
            tor_msg_t out_msg;
            memset(&out_msg, 0, sizeof(out_msg));
            out_msg.header.type = TOR_MSG_DATA;
            uint16_t frame_len = 0;

            if(!gen_encrypted_framed_message((uint8_t*)&in_msg, (uint16_t)(sizeof(tor_header_t) + ntohs(in_msg.header.payload_len)), session->hop_key, out_msg.payload, &frame_len))
            {
                retval = false;
            }
            else
            {
                out_msg.header.payload_len = htons(frame_len);
                if(!tor_send_msg(session->last_fd, &out_msg))
                {
                    retval = false;
                }
            }
        }
        else
        {
            if(!tor_send_msg(session->last_fd, &in_msg))
            {
                retval = false;
            }
        }
    }
    return retval;
}

static bool handle_poll_errors(struct pollfd *fds)
{
    bool retval = true;
    if(fds[0].revents & (POLLERR | POLLHUP | POLLNVAL))
    {
        retval = false;
    }
    else
    {
        if(fds[1].fd >= 0)
        {
            if((fds[1].revents & (POLLERR | POLLHUP | POLLNVAL)))
            {
                retval = false;
            }
        }
    }
    return retval;
}

static bool handle_poll_input(session_t *session, struct pollfd *fds)
{
    bool retval = true;
    if(fds[0].revents & POLLIN)
    {
        if(!forward_last_to_next(session))
        {
            retval = false;
        }
    }

    if(retval)
    {
        if(fds[1].fd >= 0)
        {
            if(fds[1].revents & POLLIN)
            {
                if(!forward_next_to_last(session))
                {
                    retval = false;
                }
            }
        }
    }
    return retval;
}

void forward_messages(session_t *session, volatile bool *run_flag)
{
    struct pollfd fds[TWO_SOCKETS];
    fds[0].events = POLLIN;
    fds[1].events = POLLIN;

    while(*run_flag)
    {
        fds[0].fd = session->last_fd;
        fds[1].fd = session->next_fd;

        int poll_result = poll(fds, TWO_SOCKETS, 50);

        if(poll_result < 0)
        {
            if(errno != EINTR)
            {
                break;
            }
        }
        else
        {
            if(poll_result > 0)
            {
                if(!handle_poll_errors(fds))
                {
                    break;
                }
                else
                {
                    if(!handle_poll_input(session, fds))
                    {
                        break;
                    }
                }
            }
        }
    }

    shutdown(session->last_fd, SHUT_RDWR);
    if(session->next_fd >= 0)
    {
        shutdown(session->next_fd, SHUT_RDWR);
    }
}

int connect_to_peer(uint32_t ip_v4_nbo, uint16_t port_nbo)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if(fd < 0)
    {
        perror("socket");
        return -1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = port_nbo;
    addr.sin_addr.s_addr = ip_v4_nbo;

    if(connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0)
    {
        perror("connect peer");
        close(fd);
        return -1;
    }

    return fd;
}