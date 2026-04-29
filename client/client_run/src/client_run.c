#define _DEFAULT_SOURCE

#include "client_run.h"
#include "db_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <openssl/rand.h>
#include <time.h>

#define PINNED_GUARD_FILE_PATH "/client_data/pinned_guard.bin"

typedef struct
{
    uint8_t identify_pub[32];
    uint8_t relay_ip[IP6_SIZE];
    uint16_t relay_port;
    time_t chosen_at;
} pinned_guard_t;

static connection_context_t g_connections[MAX_ACTIVE_CONNECTIONS];
static pthread_mutex_t g_conn_mutex = PTHREAD_MUTEX_INITIALIZER;
static volatile bool g_manager_run_flag = true;
static pthread_t g_accept_thread_id;
static int g_listen_fd = INVALID_SOCKET_FD;
static bool g_is_service_mode = false;
static client_core_event_callback_t g_event_callback = NULL;
static bool g_random_seeded = false;
bool g_client_defense_active = false;

/* הזהות הקבועה שלנו */
static uint8_t g_my_pub[32];
static uint8_t g_my_priv[32];

static void* worker_read_thread(void* arg);
static void* accept_clients_thread(void* arg);
static bool start_worker_thread(int connection_index);
static bool read_line(char *buf, size_t cap);
static bool init_identity_and_db(const char *db_password);
static bool start_accept_thread(void);
static int start_listen_socket(uint16_t port_host);
static bool initiate_circuit_connection_internal(const char* ip_str,
                                                 uint16_t port,
                                                 bool use_custom_route,
                                                 const int route_indices[CLIENT_ROUTE_LEN],
                                                 bool print_messages,
                                                 int *out_connection_id);
static void initiate_circuit_connection(const char* ip_str, uint16_t port);
static void init_connections_manager(void);
static void stop_connections_manager(void);
static int add_new_connection(int new_socket_fd);
static void close_connection(int connection_index);
static void print_active_connections(void);
static bool reply_to_client(int connection_index, const char* message, bool save_to_db);
static void handle_client_message(int connection_index, connection_context_t* client, tor_msg_t* msg);
static void run_chat_interface(void);

static void client_log(const char *format, ...)
{
    va_list args;
    FILE *stream = stdout;

    if (g_is_service_mode == true)
    {
        stream = stderr;
    }

    va_start(args, format);
    vfprintf(stream, format, args);
    va_end(args);

    fflush(stream);
}

static void emit_core_event(client_core_event_type_e type,
                            int connection_id,
                            int chat_id,
                            const char *text)
{
    client_core_event_t event;

    if (g_event_callback != NULL)
    {
        memset(&event, 0, sizeof(event));
        event.type = type;
        event.connection_id = connection_id;
        event.chat_id = chat_id;

        if (text != NULL)
        {
            strncpy(event.text, text, sizeof(event.text) - 1);
        }

        g_event_callback(&event);
    }
}

static void seed_random_once(void)
{
    if(g_random_seeded == false)
    {
        srand((unsigned int)time(NULL));
        g_random_seeded = true;
    }
}

static bool save_pinned_guard(const pinned_guard_t* pinned_guard)
{
    bool retval = false;
    FILE* pinned_guard_file = NULL;

    if(pinned_guard != NULL)
    {
        pinned_guard_file = fopen(PINNED_GUARD_FILE_PATH, "wb");
        if(pinned_guard_file != NULL)
        {
            if(fwrite(pinned_guard, sizeof(pinned_guard_t), 1, pinned_guard_file) == 1)
            {
                retval = true;
            }
            fclose(pinned_guard_file);
        }
    }

    return retval;
}

static bool load_pinned_guard(pinned_guard_t* pinned_guard)
{
    bool retval = false;
    FILE* pinned_guard_file = NULL;

    if(pinned_guard != NULL)
    {
        pinned_guard_file = fopen(PINNED_GUARD_FILE_PATH, "rb");
        if(pinned_guard_file != NULL)
        {
            if(fread(pinned_guard, sizeof(pinned_guard_t), 1, pinned_guard_file) == 1)
            {
                retval = true;
            }
            fclose(pinned_guard_file);
        }
    }

    return retval;
}

static int find_relay_index_by_identity(relay_decript_t* relays, uint32_t relays_amount, const uint8_t identify_pub[32])
{
    int retval = ERROR_CONNECTION_INDEX;

    for(uint32_t index = 0; index < relays_amount; index++)
    {
        if(memcmp(relays[index].identify_pub, identify_pub, 32) == 0)
        {
            retval = (int)index;
            break;
        }
    }

    return retval;
}

static int select_random_guard_index(relay_decript_t* relays, uint32_t relays_amount)
{
    int retval = ERROR_CONNECTION_INDEX;
    int guard_candidates[MAX_RELAY_BATCH_SIZE];
    uint32_t guard_candidates_count = 0;

    seed_random_once();

    for(uint32_t index = 0; index < relays_amount; index++)
    {
        if(relays[index].is_guard == true)
        {
            guard_candidates[guard_candidates_count] = (int)index;
            guard_candidates_count++;
        }
    }

    if(guard_candidates_count > 0)
    {
        retval = guard_candidates[rand() % guard_candidates_count];
    }

    return retval;
}

static int select_random_non_guard_index(relay_decript_t* relays,
                                         uint32_t relays_amount,
                                         int forbidden_a,
                                         int forbidden_b,
                                         bool require_exit)
{
    int retval = ERROR_CONNECTION_INDEX;
    int candidates[MAX_RELAY_BATCH_SIZE];
    uint32_t candidates_count = 0;

    seed_random_once();

    for(uint32_t index = 0; index < relays_amount; index++)
    {
        bool can_use = true;

        if((int)index == forbidden_a || (int)index == forbidden_b)
        {
            can_use = false;
        }

        if(can_use == true)
        {
            if(require_exit == true)
            {
                if(relays[index].is_exit == false)
                {
                    can_use = false;
                }
            }
        }

        if(can_use == true)
        {
            candidates[candidates_count] = (int)index;
            candidates_count++;
        }
    }

    if(candidates_count > 0)
    {
        retval = candidates[rand() % candidates_count];
    }

    return retval;
}

static bool select_default_route_with_pinned_guard(relay_decript_t* relays,
                                                   uint32_t relays_amount,
                                                   int route_indices[CLIENT_ROUTE_LEN],
                                                   bool* used_existing_pinned_guard)
{
    bool retval = true;
    pinned_guard_t pinned_guard;
    bool has_pinned_guard = false;
    int selected_guard_index = ERROR_CONNECTION_INDEX;
    int selected_middle_index = ERROR_CONNECTION_INDEX;
    int selected_exit_index = ERROR_CONNECTION_INDEX;

    memset(&pinned_guard, 0, sizeof(pinned_guard_t));

    if(used_existing_pinned_guard != NULL)
    {
        *used_existing_pinned_guard = false;
    }

    has_pinned_guard = load_pinned_guard(&pinned_guard);

    if(has_pinned_guard == true)
    {
        selected_guard_index = find_relay_index_by_identity(relays, relays_amount, pinned_guard.identify_pub);

        if(selected_guard_index >= 0)
        {
            if(used_existing_pinned_guard != NULL)
            {
                *used_existing_pinned_guard = true;
            }
        }
        else
        {
            has_pinned_guard = false;
        }
    }

    if(has_pinned_guard == false)
    {
        selected_guard_index = select_random_guard_index(relays, relays_amount);

        if(selected_guard_index < 0)
        {
            retval = false;
        }
        else
        {
            memset(&pinned_guard, 0, sizeof(pinned_guard_t));
            memcpy(pinned_guard.identify_pub, relays[selected_guard_index].identify_pub, 32);
            memcpy(pinned_guard.relay_ip, relays[selected_guard_index].relay_ip, IP6_SIZE);
            pinned_guard.relay_port = relays[selected_guard_index].relay_port;
            pinned_guard.chosen_at = time(NULL);

            if(save_pinned_guard(&pinned_guard) == false)
            {
                retval = false;
            }
        }
    }

    if(retval == true)
    {
        selected_middle_index = select_random_non_guard_index(relays,
                                                              relays_amount,
                                                              selected_guard_index,
                                                              ERROR_CONNECTION_INDEX,
                                                              false);

        if(selected_middle_index < 0)
        {
            retval = false;
        }
    }

    if(retval == true)
    {
        selected_exit_index = select_random_non_guard_index(relays,
                                                            relays_amount,
                                                            selected_guard_index,
                                                            selected_middle_index,
                                                            true);

        if(selected_exit_index < 0)
        {
            retval = false;
        }
    }

    if(retval == true)
    {
        route_indices[0] = selected_guard_index;
        route_indices[1] = selected_middle_index;
        route_indices[2] = selected_exit_index;
    }

    return retval;
}

void client_core_set_service_mode(bool is_service_mode)
{
    g_is_service_mode = is_service_mode;
}

void client_core_set_event_callback(client_core_event_callback_t callback)
{
    g_event_callback = callback;
}

static void init_connections_manager(void)
{
    for (int index = 0; index < MAX_ACTIVE_CONNECTIONS; index++)
    {
        g_connections[index].socket_fd = INVALID_SOCKET_FD;
        g_connections[index].is_active = false;
        g_connections[index].is_initiator = false;
        g_connections[index].is_recv_ready = false;
        g_connections[index].is_send_ready = false;
        g_connections[index].peer_db_id = -1;
        g_connections[index].peer_ip_nbo = 0;
        g_connections[index].peer_port_nbo = 0;
        memset(&g_connections[index].circuit, 0, sizeof(circuit_t));
        memset(g_connections[index].e2e_recv_key, 0, E2E_KEY_LEN);
        memset(g_connections[index].e2e_send_key, 0, E2E_KEY_LEN);
    }
}

static void stop_connections_manager(void)
{
    g_manager_run_flag = false;

    pthread_mutex_lock(&g_conn_mutex);
    for (int index = 0; index < MAX_ACTIVE_CONNECTIONS; index++)
    {
        if (g_connections[index].is_active == true)
        {
            shutdown(g_connections[index].socket_fd, SHUT_RDWR);
        }
    }
    pthread_mutex_unlock(&g_conn_mutex);
}

static int add_new_connection(int new_socket_fd)
{
    int retval = ERROR_CONNECTION_INDEX;

    pthread_mutex_lock(&g_conn_mutex);

    for (int index = 0; index < MAX_ACTIVE_CONNECTIONS; index++)
    {
        if (g_connections[index].is_active == false)
        {
            g_connections[index].socket_fd = new_socket_fd;
            g_connections[index].is_active = true;
            g_connections[index].is_initiator = false;
            g_connections[index].is_recv_ready = false;
            g_connections[index].is_send_ready = false;
            g_connections[index].peer_db_id = -1;
            g_connections[index].peer_ip_nbo = 0;
            g_connections[index].peer_port_nbo = 0;
            retval = index;
            break;
        }
    }

    pthread_mutex_unlock(&g_conn_mutex);
    return retval;
}

static void close_connection(int connection_index)
{
    if (connection_index >= 0)
    {
        if (connection_index < MAX_ACTIVE_CONNECTIONS)
        {
            pthread_mutex_lock(&g_conn_mutex);
            
            if (g_connections[connection_index].is_active == true)
            {
                close(g_connections[connection_index].socket_fd);
                g_connections[connection_index].is_active = false;
                g_connections[connection_index].socket_fd = INVALID_SOCKET_FD;
                g_connections[connection_index].is_recv_ready = false;
                g_connections[connection_index].is_send_ready = false;
            }
            
            pthread_mutex_unlock(&g_conn_mutex);
        }
    }
}

static void print_active_connections(void)
{
    pthread_mutex_lock(&g_conn_mutex);
    
    client_log("\n--- Active Live Connections ---\n");
    for (int index = 0; index < MAX_ACTIVE_CONNECTIONS; index++)
    {
        if (g_connections[index].is_active == true)
        {
            client_log("Connection ID: %d | Chat DB ID: %d | Status: %s\n", 
                       index,
                       g_connections[index].peer_db_id, 
                       (g_connections[index].is_initiator ? "Outgoing Circuit" : "Incoming Connection"));
        }
    }
    client_log("-------------------------------\n> ");
    
    pthread_mutex_unlock(&g_conn_mutex);
}

static bool reply_to_client(int connection_index, const char* message, bool save_to_db)
{
    bool retval = false;
    connection_context_t* current_client = NULL;

    if (connection_index >= 0 && connection_index < MAX_ACTIVE_CONNECTIONS)
    {
        pthread_mutex_lock(&g_conn_mutex);

        current_client = &g_connections[connection_index];

        if (current_client->is_active == true && current_client->is_send_ready == true)
        {
            uint16_t msg_len = (uint16_t)strlen(message);

            if (current_client->is_initiator == true)
            {
                bool is_sent = chat_send_encrypted(&current_client->circuit, 
                                                   current_client->peer_ip_nbo, 
                                                   current_client->peer_port_nbo, 
                                                   current_client->e2e_send_key, 
                                                   (const uint8_t*)message, 
                                                   msg_len);
                if (is_sent == true)
                {
                    retval = true;
                }
            }
            else
            {
                uint8_t cipher[TOR_MSG_SIZE];
                uint16_t cipher_len = 0;
                bool is_encrypted = gen_encrypted_framed_message((const uint8_t*)message,
                                                                 msg_len,
                                                                 current_client->e2e_send_key,
                                                                 cipher,
                                                                 &cipher_len);
                if (is_encrypted == true)
                {
                    tor_msg_t outgoing_msg;
                    memset(&outgoing_msg, 0, sizeof(outgoing_msg));
                    outgoing_msg.header.type = TOR_MSG_DATA;
                    outgoing_msg.header.payload_len = htons(cipher_len);
                    memcpy(outgoing_msg.payload, cipher, cipher_len);

                    if (tor_send_msg(current_client->socket_fd, &outgoing_msg) == true)
                    {
                        retval = true;
                    }
                }
            }
        }

        pthread_mutex_unlock(&g_conn_mutex);

        if (retval == true && save_to_db == true)
        {
            if (current_client != NULL)
            {
                db_save_message(current_client->peer_db_id, message, true);
            }
        }
    }

    return retval;
}

static void handle_client_message(int connection_index, connection_context_t* client, tor_msg_t* msg)
{
    if (msg->header.type == TOR_MSG_DATA)
    {
        uint16_t payload_len = ntohs(msg->header.payload_len);
        uint16_t expected_keyx_len = (uint16_t)(MAGIC_KEYX_LEN + E2E_KEY_LEN + 32);

        if (payload_len == expected_keyx_len)
        {
            if (memcmp(msg->payload, MAGIC_KEYX_STR, MAGIC_KEYX_LEN) == 0)
            {
                memcpy(client->e2e_recv_key, msg->payload + MAGIC_KEYX_LEN, E2E_KEY_LEN);

                {
                    uint8_t peer_ident[32];
                    memcpy(peer_ident, msg->payload + MAGIC_KEYX_LEN + E2E_KEY_LEN, 32);
                    client->peer_db_id = db_get_or_create_peer(peer_ident);
                }

                client->is_recv_ready = true;
                memcpy(client->e2e_send_key, client->e2e_recv_key, E2E_KEY_LEN);
                client->is_send_ready = true;

                emit_core_event(CLIENT_CORE_EVENT_CONNECTED,
                                connection_index,
                                client->peer_db_id,
                                "");

                if (client->is_initiator == true)
                {
                    client_log("\n[Client on FD %d] Handshake ACK received. Chat DB ID: %d.\n> ",
                               client->socket_fd,
                               client->peer_db_id);
                }
                else
                {
                    client_log("\n[Client on FD %d] Handshake received. Chat DB ID: %d. Sent ACK back.\n> ",
                               client->socket_fd,
                               client->peer_db_id);

                    tor_msg_t reply_msg;
                    memset(&reply_msg, 0, sizeof(reply_msg));
                    reply_msg.header.type = TOR_MSG_DATA;
                    reply_msg.header.payload_len = htons(expected_keyx_len);
                    memcpy(reply_msg.payload, MAGIC_KEYX_STR, MAGIC_KEYX_LEN);
                    memcpy(reply_msg.payload + MAGIC_KEYX_LEN, client->e2e_send_key, E2E_KEY_LEN);
                    memcpy(reply_msg.payload + MAGIC_KEYX_LEN + E2E_KEY_LEN, g_my_pub, 32);

                    tor_send_msg(client->socket_fd, &reply_msg);
                }
            }
        }
        else
        {
            if (client->is_recv_ready == true)
            {
                uint8_t plaintext[TOR_MSG_SIZE];
                uint16_t blob_len = payload_len;
                uint16_t out_len = 0;
                bool is_decrypted = decrypt_blob((uint8_t*)msg->payload,
                                                 &blob_len,
                                                 client->e2e_recv_key,
                                                 plaintext,
                                                 &out_len);
                if (is_decrypted == true)
                {
                    if (out_len < TOR_MSG_SIZE)
                    {
                        plaintext[out_len] = '\0';

                        if (strcmp((char*)plaintext, "PING") == 0)
                        {
                            reply_to_client(connection_index, "PONG", false);
                        }
                        else if (strcmp((char*)plaintext, "PONG") == 0)
                        {
                            emit_core_event(CLIENT_CORE_EVENT_PING_OK,
                                            connection_index,
                                            client->peer_db_id,
                                            "");

                            client_log("\n[System] Connection %d is ALIVE (Ping successful) \n> ", connection_index);
                        }
                        else
                        {
                            emit_core_event(CLIENT_CORE_EVENT_INCOMING_MESSAGE,
                                            connection_index,
                                            client->peer_db_id,
                                            (const char*)plaintext);

                            client_log("\n[Peer %d] %s\n> ", client->peer_db_id, (char*)plaintext);
                            db_save_message(client->peer_db_id, (const char*)plaintext, false);
                        }
                    }
                }
            }
        }
    }
}

static bool read_line(char *buf, size_t cap)
{
    bool retval = false;

    if (fgets(buf, cap, stdin) != NULL)
    {
        buf[strcspn(buf, "\n")] = '\0';
        retval = true;
    }

    return retval;
}

static bool init_identity_and_db(const char *db_password)
{
    bool retval = true;
    bool found_id = false;

    if (db_password == NULL)
    {
        retval = false;
    }

    if (retval == true)
    {
        if (db_init("/client_data/chat_history.db", db_password) == false)
        {
            client_log("[System] Critical Error: Failed to initialize/decrypt the database.\n");
            retval = false;
        }
        else
        {
            client_log("[System] Database unlocked and ready.\n");
        }
    }

    if (retval == true)
    {
        if (db_load_identity(g_my_pub, g_my_priv, &found_id) == false)
        {
            retval = false;
        }
    }

    if (retval == true)
    {
        if (found_id == false)
        {
            client_log("[System] Generating new permanent Tor Identity...\n");

            if (generate_empherial_keypair(g_my_pub, g_my_priv) == false)
            {
                retval = false;
            }
            else
            {
                if (db_save_identity(g_my_pub, g_my_priv) == false)
                {
                    retval = false;
                }
            }
        }
    }

    if (retval == true)
    {
        client_log("[System] Your Public Identity: %02X%02X%02X%02X...\n",
                   g_my_pub[0],
                   g_my_pub[1],
                   g_my_pub[2],
                   g_my_pub[3]);
    }

    return retval;
}

static bool start_worker_thread(int connection_index)
{
    bool retval = false;
    int* thread_arg = NULL;
    pthread_t worker_thread;
    int create_res = FAILURE;

    thread_arg = malloc(sizeof(int));
    if (thread_arg != NULL)
    {
        *thread_arg = connection_index;
        create_res = pthread_create(&worker_thread, NULL, worker_read_thread, thread_arg);

        if (create_res == SUCCESS)
        {
            pthread_detach(worker_thread);
            retval = true;
        }
        else
        {
            free(thread_arg);
        }
    }

    return retval;
}

static bool start_accept_thread(void)
{
    bool retval = true;
    int thread_create_res = SUCCESS;

    thread_create_res = pthread_create(&g_accept_thread_id, NULL, accept_clients_thread, &g_listen_fd);
    if (thread_create_res != SUCCESS)
    {
        retval = false;
    }

    return retval;
}

static bool initiate_circuit_connection_internal(const char* ip_str,
                                                 uint16_t port,
                                                 bool use_custom_route,
                                                 const int route_indices[CLIENT_ROUTE_LEN],
                                                 bool print_messages,
                                                 int *out_connection_id)
{
    bool retval = true;
    struct in_addr peer_addr;
    circuit_t new_circuit;
    uint8_t new_e2e_key[E2E_KEY_LEN];
    int new_index = ERROR_CONNECTION_INDEX;
    uint32_t relays_amount = 0;
    relay_decript_t* relays = NULL;
    relay_decript_t chosen_relays[CIRCUIT_LEN];
    int rand_res = OPENSSL_SUCCESS;
    uint32_t peer_ip_nbo = 0;
    uint16_t peer_port_nbo = 0;
    int selected_route_indices[CLIENT_ROUTE_LEN];
    bool used_existing_pinned_guard = false;

    selected_route_indices[0] = 0;
    selected_route_indices[1] = 1;
    selected_route_indices[2] = 2;

    if (out_connection_id != NULL)
    {
        *out_connection_id = ERROR_CONNECTION_INDEX;
    }

    if (inet_pton(AF_INET, ip_str, &peer_addr) != 1)
    {
        retval = false;
    }

    if (retval == true)
    {
        relays = fetch_relays_from_dir(DIR_SERVER_CFG_PATH, &relays_amount);

        if (relays == NULL || relays_amount < CIRCUIT_LEN)
        {
            retval = false;
        }
    }

    if (retval == true)
    {
        if (use_custom_route == true)
        {
            if (route_indices == NULL)
            {
                retval = false;
            }
            else
            {
                if (route_indices[0] >= 0 && route_indices[0] < (int)relays_amount &&
                    route_indices[1] >= 0 && route_indices[1] < (int)relays_amount &&
                    route_indices[2] >= 0 && route_indices[2] < (int)relays_amount)
                {
                    selected_route_indices[0] = route_indices[0];
                    selected_route_indices[1] = route_indices[1];
                    selected_route_indices[2] = route_indices[2];
                }
                else
                {
                    retval = false;
                }
            }
        }
        else
        {
            retval = select_default_route_with_pinned_guard(relays,
                                                           relays_amount,
                                                           selected_route_indices,
                                                           &used_existing_pinned_guard);
        }
    }

    if (retval == true)
    {
        chosen_relays[0] = relays[selected_route_indices[0]];
        chosen_relays[1] = relays[selected_route_indices[1]];
        chosen_relays[2] = relays[selected_route_indices[2]];
    }

    if (retval == true)
    {
        retval = build_circuit_with_relays(&new_circuit, chosen_relays);
    }

    if (retval == true)
    {
        notify_directory_of_route(DIR_SERVER_CFG_PATH,
                                  chosen_relays[0].relay_id,
                                  chosen_relays[1].relay_id,
                                  chosen_relays[2].relay_id);

        rand_res = RAND_bytes(new_e2e_key, E2E_KEY_LEN);
        if (rand_res != OPENSSL_SUCCESS)
        {
            retval = false;
        }
    }

    if (retval == true)
    {
        peer_ip_nbo = peer_addr.s_addr;
        peer_port_nbo = htons(port);

        retval = chat_send_keyx(&new_circuit,
                                peer_ip_nbo,
                                peer_port_nbo,
                                new_e2e_key,
                                g_my_pub);
    }

    if (retval == true)
    {
        new_index = add_new_connection(new_circuit.guard_fd);
        if (new_index == ERROR_CONNECTION_INDEX)
        {
            retval = false;
        }
    }

    if (retval == true)
    {
        pthread_mutex_lock(&g_conn_mutex);

        g_connections[new_index].is_initiator = true;
        g_connections[new_index].circuit = new_circuit;
        g_connections[new_index].peer_ip_nbo = peer_ip_nbo;
        g_connections[new_index].peer_port_nbo = peer_port_nbo;

        memcpy(g_connections[new_index].e2e_send_key, new_e2e_key, E2E_KEY_LEN);
        memcpy(g_connections[new_index].e2e_recv_key, new_e2e_key, E2E_KEY_LEN);

        g_connections[new_index].is_send_ready = true;
        g_connections[new_index].is_recv_ready = true;

        pthread_mutex_unlock(&g_conn_mutex);

        if (start_worker_thread(new_index) == false)
        {
            close_connection(new_index);
            retval = false;
        }
    }

    if (relays != NULL)
    {
        free(relays);
    }

    if (retval == true)
    {
        if (out_connection_id != NULL)
        {
            *out_connection_id = new_index;
        }

        emit_core_event(CLIENT_CORE_EVENT_CONNECTED,
                        new_index,
                        -1,
                        "");
    }

    if (print_messages == true)
    {
        if (retval == true)
        {
            if(use_custom_route == false)
            {
                if(used_existing_pinned_guard == true)
                {
                    client_log("[System] Connected! Circuit built (Connection ID %d) using pinned Guard.\n> ", new_index);
                }
                else
                {
                    client_log("[System] Connected! Circuit built (Connection ID %d) using a newly selected pinned Guard.\n> ", new_index);
                }
            }
            else
            {
                client_log("[System] Connected! Circuit built (Connection ID %d).\n> ", new_index);
            }
        }
        else
        {
            client_log("[System] Failed to build connection.\n> ");
        }
    }

    return retval;
}

static void initiate_circuit_connection(const char* ip_str, uint16_t port)
{
    bool is_success = true;
    uint32_t relays_amount = 0;
    relay_decript_t* relays = NULL;
    char choice[INPUT_LINE_SIZE];
    char custom_indices[INPUT_LINE_SIZE];
    int route_indices[CLIENT_ROUTE_LEN];
    pinned_guard_t pinned_guard;
    bool has_pinned_guard = false;

    route_indices[0] = 0;
    route_indices[1] = 1;
    route_indices[2] = 2;

    memset(&pinned_guard, 0, sizeof(pinned_guard_t));
    has_pinned_guard = load_pinned_guard(&pinned_guard);

    relays = fetch_relays_from_dir(DIR_SERVER_CFG_PATH, &relays_amount);
    
    if (relays == NULL || relays_amount < CIRCUIT_LEN)
    {
        is_success = false;
    }

    if (is_success == true)
    {
        client_log("\n[System] Available Relays (Guard-aware export):\n");
        for (uint32_t index = 0; index < relays_amount; index++)
        {
            client_log("  [%d] IP: %u.%u.%u.%u | Port: %u | Active Load: %u | Guard: %s | Exit: %s | Malicious(sim): %s\n",
                       index,
                       relays[index].relay_ip[0],
                       relays[index].relay_ip[1],
                       relays[index].relay_ip[2],
                       relays[index].relay_ip[3],
                       relays[index].relay_port,
                       relays[index].assigned_count,
                       (relays[index].is_guard ? "yes" : "no"),
                       (relays[index].is_exit ? "yes" : "no"),
                       (relays[index].is_malicious ? "yes" : "no"));
        }

        if(has_pinned_guard == true)
        {
            client_log("[System] A pinned Guard already exists locally.\n");
        }
        else
        {
            client_log("[System] No pinned Guard found locally.\n");
        }

        client_log("\n[System] Press ENTER to build a default protected circuit, or 'c' to build a custom route: ");

        read_line(choice, sizeof(choice));

        if (choice[0] == 'c' || choice[0] == 'C')
        {
            client_log("[System] Enter 3 relay indices separated by space (e.g. '0 2 1'): ");

            read_line(custom_indices, sizeof(custom_indices));

            if (sscanf(custom_indices,
                       "%d %d %d",
                       &route_indices[0],
                       &route_indices[1],
                       &route_indices[2]) == 3)
            {
                initiate_circuit_connection_internal(ip_str,
                                                     port,
                                                     true,
                                                     route_indices,
                                                     true,
                                                     NULL);
            }
            else
            {
                client_log("[System] Invalid input.\n> ");
            }
        }
        else
        {
            initiate_circuit_connection_internal(ip_str,
                                                 port,
                                                 false,
                                                 NULL,
                                                 true,
                                                 NULL);
        }
    }
    else
    {
        client_log("[System] Failed to fetch enough relays from directory.\n> ");
    }

    if (relays != NULL)
    {
        free(relays);
    }
}

static void* worker_read_thread(void* arg)
{
    void* retval = NULL;
    int connection_index = ERROR_CONNECTION_INDEX;

    if (arg != NULL)
    {
        connection_index = *(int*)arg;
        free(arg);
    }

    if (connection_index != ERROR_CONNECTION_INDEX)
    {
        connection_context_t* current_client = &g_connections[connection_index];
        tor_msg_t incoming_msg;
        bool is_client_connected = true;

        while (is_client_connected == true)
        {
            if (current_client->is_active == false)
            {
                is_client_connected = false;
            }

            if (is_client_connected == true)
            {
                if (tor_recv_msg(current_client->socket_fd, &incoming_msg) == false)
                {
                    is_client_connected = false;
                }
                else
                {
                    if (current_client->is_initiator == true)
                    {
                        uint8_t plain_data[TOR_MSG_SIZE];
                        uint16_t plain_len = 0;
                        bool is_peeled = recv_onion_data(&current_client->circuit,
                                                         &incoming_msg,
                                                         plain_data,
                                                         &plain_len);
                        if (is_peeled == true)
                        {
                            tor_msg_t* peeled_msg = (tor_msg_t*)plain_data;
                            handle_client_message(connection_index, current_client, peeled_msg);
                        }
                    }
                    else
                    {
                        handle_client_message(connection_index, current_client, &incoming_msg);
                    }
                }
            }
        }

        if (current_client->peer_ip_nbo != 0)
        {
            struct in_addr ip_addr;
            char ip_str[INET_ADDRSTRLEN];
            uint16_t port = 0;
            bool was_initiator = false;

            ip_addr.s_addr = current_client->peer_ip_nbo;
            inet_ntop(AF_INET, &ip_addr, ip_str, INET_ADDRSTRLEN);
            port = ntohs(current_client->peer_port_nbo);
            was_initiator = current_client->is_initiator;

            close_connection(connection_index);

            emit_core_event(CLIENT_CORE_EVENT_CONNECTION_DROPPED,
                            connection_index,
                            current_client->peer_db_id,
                            "relay_crash_or_peer_disconnect");

            client_log("\n[!!! SYSTEM ALERT !!!] Connection %d has dropped!\n", connection_index);
            client_log("Reason: A relay in your circuit crashed, or the peer disconnected.\n");

            if (was_initiator == true)
            {
                client_log("[System] To build a NEW secure route to this destination, copy and run:\n");
                client_log("         /connect %s %u\n> ", ip_str, port);
            }
            else
            {
                client_log("[System] Waiting for the peer to rebuild their route and reconnect to you...\n> ");
            }
        }
        else
        {
            close_connection(connection_index);

            emit_core_event(CLIENT_CORE_EVENT_CONNECTION_DROPPED,
                            connection_index,
                            current_client->peer_db_id,
                            "peer_disconnected");

            client_log("\n[System] Connection %d disconnected.\n> ", connection_index);
        }
    }

    return retval;
}

static void* accept_clients_thread(void* arg)
{
    void* retval = NULL;
    int listen_socket_fd = INVALID_SOCKET_FD;

    if (arg != NULL)
    {
        listen_socket_fd = *(int*)arg;
    }

    if (listen_socket_fd != INVALID_SOCKET_FD)
    {
        while (g_manager_run_flag == true)
        {
            int new_client_fd = accept(listen_socket_fd, NULL, NULL);
            bool is_valid_client = (new_client_fd >= 0);
            int new_index = ERROR_CONNECTION_INDEX;

            if (is_valid_client == true)
            {
                new_index = add_new_connection(new_client_fd);
                
                if (new_index == ERROR_CONNECTION_INDEX)
                {
                    close(new_client_fd);
                    is_valid_client = false;
                }
            }

            if (is_valid_client == true)
            {
                if (start_worker_thread(new_index) == false)
                {
                    close_connection(new_index);
                }
            }
        }
    }

    return retval;
}

static int start_listen_socket(uint16_t port_host)
{
    int retval = INVALID_SOCKET_FD;
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (listen_fd >= 0)
    {
        int one = 1;
        setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port_host);
        addr.sin_addr.s_addr = htonl(INADDR_ANY);

        if (bind(listen_fd, (struct sockaddr*)&addr, sizeof(addr)) == 0)
        {
            if (listen(listen_fd, LISTEN_BACKLOG) == 0)
            {
                client_log("[Client] Listening on 0.0.0.0:%u\n", port_host);
                retval = listen_fd;
            }
            else
            {
                close(listen_fd);
            }
        }
        else
        {
            close(listen_fd);
        }
    }

    return retval;
}

static void run_chat_interface(void)
{
    char chat_line[CHAT_LINE_SIZE];
    bool is_running = true;

    client_log("[System] Chat interface ready.\n");
    client_log("[System] Commands: %s, %s, %s<IP> <Port>, %s<Conn_ID> <msg>, %s<Conn_ID>, %s<Conn_ID>, %s<Chat_ID>,/defense on, /defense off, %s\n", 
               CMD_CLIENTS,
               CMD_CHATS,
               CMD_CONNECT_PREFIX,
               CMD_SEND_PREFIX,
               CMD_PING_PREFIX,
               "/disconnect ",
               CMD_HISTORY_PREFIX,
               CMD_QUIT);
            
    while (is_running == true)
    {
        client_log("> ");

        if (read_line(chat_line, sizeof(chat_line)) == false)
        {
            is_running = false;
        }

        if (is_running == true)
        {
            if (strcmp(chat_line, CMD_QUIT) == 0)
            {
                is_running = false;
            }
            else if (strcmp(chat_line, CMD_CLIENTS) == 0)
            {
                print_active_connections();
            }
            else if (strcmp(chat_line, CMD_CHATS) == 0)
            {
                db_print_all_chats();
            }
            else if (strncmp(chat_line, CMD_CONNECT_PREFIX, CMD_CONNECT_PREFIX_LEN) == 0)
            {
                char target_ip[INPUT_LINE_SIZE];
                int target_port = 0;

                if (sscanf(chat_line + CMD_CONNECT_PREFIX_LEN, "%s %d", target_ip, &target_port) == EXPECTED_PARSED_CONNECT)
                {
                    if (target_port >= MIN_PORT_NUMBER && target_port <= MAX_PORT_NUMBER)
                    {
                        initiate_circuit_connection(target_ip, (uint16_t)target_port);
                    }
                }
                else
                {
                    client_log("[System] Usage: %s<IP> <Port>\n", CMD_CONNECT_PREFIX);
                }
            }
            else if (strncmp(chat_line, CMD_SEND_PREFIX, CMD_SEND_PREFIX_LEN) == 0)
            {
                int target_id = 0;
                char message_content[CHAT_LINE_SIZE];

                if (sscanf(chat_line + CMD_SEND_PREFIX_LEN, "%d %[^\n]", &target_id, message_content) == EXPECTED_PARSED_SEND)
                {
                    if (reply_to_client(target_id, message_content, true) == false)
                    {
                        client_log("[System] Failed to send message to connection %d.\n", target_id);
                    }
                }
                else
                {
                    client_log("[System] Usage: %s<Connection_ID> <message>\n", CMD_SEND_PREFIX);
                }
            }
            else if (strncmp(chat_line, CMD_PING_PREFIX, CMD_PING_PREFIX_LEN) == 0)
            {
                int target_id = 0;

                if (sscanf(chat_line + CMD_PING_PREFIX_LEN, "%d", &target_id) == 1)
                {
                    client_log("[System] Pinging connection %d...\n", target_id);

                    if (reply_to_client(target_id, "PING", false) == false)
                    {
                        client_log("[System] Failed to send PING (Connection is DEAD).\n");
                    }
                }
                else
                {
                    client_log("[System] Usage: %s<Connection_ID>\n", CMD_PING_PREFIX);
                }
            }
            else if (strncmp(chat_line, "/disconnect ", 12) == 0)
            {
                int target_id = 0;

                if (sscanf(chat_line + 12, "%d", &target_id) == 1)
                {
                    close_connection(target_id);
                    client_log("[System] Connection %d closed manually. Route discarded.\n", target_id);
                }
                else
                {
                    client_log("[System] Usage: /disconnect <Connection_ID>\n");
                }
            }
            else if (strncmp(chat_line, CMD_HISTORY_PREFIX, CMD_HISTORY_PREFIX_LEN) == 0)
            {
                int target_id = 0;

                if (sscanf(chat_line + CMD_HISTORY_PREFIX_LEN, "%d", &target_id) == 1)
                {
                    db_print_chat_history(target_id);
                }
                else
                {
                    client_log("[System] Usage: %s<Chat_ID>\n", CMD_HISTORY_PREFIX);
                }
            }
            else if (strcmp(chat_line, "/defense on") == 0)
            {
                g_client_defense_active = true;
                client_log("[System] Defense mode is now ON. Circuit hijacking will be blocked.\n");
            }
            else if (strcmp(chat_line, "/defense off") == 0)
            {
                g_client_defense_active = false;
                client_log("[System] Defense mode is now OFF. Vulnerable to circuit hijacking.\n");
            }
            else
            {
                if (chat_line[0] != '\0')
                {
                    client_log("[System] Unknown command.\n");
                }
            }
        }
    }
}

bool client_core_init(uint16_t local_port, const char *db_password)
{
    bool retval = true;

    if (init_identity_and_db(db_password) == false)
    {
        retval = false;
    }

    if (retval == true)
    {
        g_listen_fd = start_listen_socket(local_port);
        if (g_listen_fd < 0)
        {
            retval = false;
        }
    }

    if (retval == true)
    {
        init_connections_manager();
        g_manager_run_flag = true;

        if (start_accept_thread() == false)
        {
            close(g_listen_fd);
            g_listen_fd = INVALID_SOCKET_FD;
            retval = false;
        }
    }

    return retval;
}

void client_core_shutdown(void)
{
    stop_connections_manager();

    if (g_listen_fd >= 0)
    {
        shutdown(g_listen_fd, SHUT_RDWR);
        close(g_listen_fd);
        g_listen_fd = INVALID_SOCKET_FD;
    }

    pthread_join(g_accept_thread_id, NULL);
    usleep(100000);

    db_close();
}

bool client_core_connect(const char *ip_str,
                         uint16_t port,
                         bool use_custom_route,
                         const int route_indices[CLIENT_ROUTE_LEN],
                         int *out_connection_id)
{
    bool retval = false;

    retval = initiate_circuit_connection_internal(ip_str,
                                                  port,
                                                  use_custom_route,
                                                  route_indices,
                                                  false,
                                                  out_connection_id);

    return retval;
}

bool client_core_send(int connection_index, const char *message)
{
    bool retval = false;

    if (message != NULL)
    {
        retval = reply_to_client(connection_index, message, true);
    }

    return retval;
}

bool client_core_ping(int connection_index)
{
    bool retval = false;

    retval = reply_to_client(connection_index, "PING", false);

    return retval;
}

bool client_core_disconnect(int connection_index)
{
    bool retval = false;

    if (connection_index >= 0 && connection_index < MAX_ACTIVE_CONNECTIONS)
    {
        close_connection(connection_index);
        retval = true;
    }

    return retval;
}

void client_core_list_connections(void)
{
    print_active_connections();
}

int start_client_application(void)
{
    int retval = EXIT_SUCCESS;
    char line[INPUT_LINE_SIZE];
    char db_password[INPUT_LINE_SIZE];
    unsigned long local_port = 0;
    bool is_success = true;

    client_core_set_service_mode(false);
    client_core_set_event_callback(NULL);

    client_log("Enter local listen port: ");

    if (read_line(line, sizeof(line)) == false)
    {
        is_success = false;
        retval = EXIT_FAILURE;
    }

    if (is_success == true)
    {
        local_port = strtoul(line, NULL, BASE_DECIMAL);

        if (local_port < MIN_PORT_NUMBER || local_port > MAX_PORT_NUMBER)
        {
            is_success = false;
            retval = EXIT_FAILURE;
        }
    }
    
    if (is_success == true)
    {
        client_log("Enter Master Password to unlock/create your secure vault: ");

        if (read_line(db_password, sizeof(db_password)) == false)
        {
            is_success = false;
            retval = EXIT_FAILURE;
        }
    }

    if (is_success == true)
    {
        is_success = client_core_init((uint16_t)local_port, db_password);
        if (is_success == false)
        {
            retval = EXIT_FAILURE;
        }
    }

    if (is_success == true)
    {
        run_chat_interface();
        client_core_shutdown();
    }

    return retval;
}