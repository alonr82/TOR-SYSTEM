#define _POSIX_C_SOURCE 200112L

#include "client_run.h"
#include "db_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <openssl/rand.h>

static connection_context_t g_connections[MAX_ACTIVE_CONNECTIONS];
static pthread_mutex_t g_conn_mutex = PTHREAD_MUTEX_INITIALIZER;
static volatile bool g_manager_run_flag = true;
static void* worker_read_thread(void* arg);

/* הזהות הקבועה שלנו */
static uint8_t g_my_pub[32];
static uint8_t g_my_priv[32];

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
    }
}

static void stop_connections_manager(void)
{
    g_manager_run_flag = false;

    pthread_mutex_lock(&g_conn_mutex);
    for (int i = 0; i < MAX_ACTIVE_CONNECTIONS; i++)
    {
        if (g_connections[i].is_active == true)
        {
            shutdown(g_connections[i].socket_fd, SHUT_RDWR);
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
            }
            
            pthread_mutex_unlock(&g_conn_mutex);
        }
    }
}

static void print_active_connections(void)
{
    pthread_mutex_lock(&g_conn_mutex);
    
    printf("\n--- Active Live Connections ---\n");
    for (int i = 0; i < MAX_ACTIVE_CONNECTIONS; i++)
    {
        if (g_connections[i].is_active == true)
        {
            printf("Connection ID: %d | Chat DB ID: %d | Status: %s\n", 
                    i, g_connections[i].peer_db_id, 
                    (g_connections[i].is_initiator ? "Outgoing Circuit" : "Incoming Connection"));
        }
    }
    printf("-------------------------------\n> ");
    fflush(stdout);
    
    pthread_mutex_unlock(&g_conn_mutex);
}

static bool reply_to_client(int connection_index, const char* message)
{
    bool retval = false;

    if (connection_index >= 0)
    {
        if (connection_index < MAX_ACTIVE_CONNECTIONS)
        {
            pthread_mutex_lock(&g_conn_mutex);
            connection_context_t* current_client = &g_connections[connection_index];
            bool is_client_ready = (current_client->is_active == true && current_client->is_send_ready == true);
            if (is_client_ready == true)
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
                    bool is_encrypted = gen_encrypted_framed_message((const uint8_t*)message, msg_len, current_client->e2e_send_key, cipher, &cipher_len);
                    if (is_encrypted == true)
                    {
                        tor_msg_t outgoing_msg;
                        memset(&outgoing_msg, 0, sizeof(outgoing_msg));
                        outgoing_msg.header.type = TOR_MSG_DATA;
                        outgoing_msg.header.payload_len = htons(cipher_len);
                        memcpy(outgoing_msg.payload, cipher, cipher_len);

                        bool is_sent = tor_send_msg(current_client->socket_fd, &outgoing_msg);
                        if (is_sent == true)
                        {
                            retval = true;
                        }
                    }
                }
            }
            pthread_mutex_unlock(&g_conn_mutex);

            if (retval == true)
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
            int is_keyx = memcmp(msg->payload, MAGIC_KEYX_STR, MAGIC_KEYX_LEN);
            if (is_keyx == 0)
            {
                memcpy(client->e2e_recv_key, msg->payload + MAGIC_KEYX_LEN, E2E_KEY_LEN);
                
                uint8_t peer_ident[32];
                memcpy(peer_ident, msg->payload + MAGIC_KEYX_LEN + E2E_KEY_LEN, 32);
                
                client->peer_db_id = db_get_or_create_peer(peer_ident);
                
                client->is_recv_ready = true;
                memcpy(client->e2e_send_key, client->e2e_recv_key, E2E_KEY_LEN);
                client->is_send_ready = true;

                if (client->is_initiator) {
                    printf("\n[Client on FD %d] Handshake ACK received. Chat DB ID: %d.\n> ", client->socket_fd, client->peer_db_id);
                } else {
                    printf("\n[Client on FD %d] Handshake received. Chat DB ID: %d. Sent ACK back.\n> ", client->socket_fd, client->peer_db_id);
                    
                    tor_msg_t reply_msg;
                    memset(&reply_msg, 0, sizeof(reply_msg));
                    reply_msg.header.type = TOR_MSG_DATA;
                    reply_msg.header.payload_len = htons(expected_keyx_len);
                    memcpy(reply_msg.payload, MAGIC_KEYX_STR, MAGIC_KEYX_LEN);
                    memcpy(reply_msg.payload + MAGIC_KEYX_LEN, client->e2e_send_key, E2E_KEY_LEN);
                    memcpy(reply_msg.payload + MAGIC_KEYX_LEN + E2E_KEY_LEN, g_my_pub, 32);
                    
                    tor_send_msg(client->socket_fd, &reply_msg);
                }
                fflush(stdout);
            }
        }
        else
        {
            if (client->is_recv_ready == true)
            {
                uint8_t plaintext[TOR_MSG_SIZE];
                uint16_t blob_len = payload_len;
                uint16_t out_len = 0;
                bool is_decrypted = decrypt_blob((uint8_t*)msg->payload, &blob_len, client->e2e_recv_key, plaintext, &out_len);
                if (is_decrypted == true)
                {
                    if (out_len < TOR_MSG_SIZE)
                    {
                        plaintext[out_len] = '\0';
                        printf("\n[Peer %d] %s\n> ", client->peer_db_id, (char*)plaintext);
                        fflush(stdout);
                        
                        db_save_message(client->peer_db_id, (const char*)plaintext, false);
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

static void initiate_circuit_connection(const char* ip_str, uint16_t port)
{
    struct in_addr peer_addr;
    bool is_success = true;
    circuit_t new_circuit;
    uint8_t new_e2e_key[E2E_KEY_LEN];
    int new_index = ERROR_CONNECTION_INDEX;

    int is_valid_ip = inet_pton(AF_INET, ip_str, &peer_addr);
    if (is_valid_ip != 1)
    {
        printf("[System] Invalid IP address.\n> ");
        fflush(stdout);
        return;
    }

    uint32_t relays_amount = 0;
    relay_decript_t* relays = fetch_relays_from_dir(DIR_SERVER_CFG_PATH, &relays_amount);
    
    if (relays == NULL || relays_amount < CIRCUIT_LEN)
    {
        printf("[System] Failed to fetch enough relays from directory (found %u).\n> ", relays_amount);
        fflush(stdout);
        if (relays) free(relays);
        return;
    }

    printf("\n[System] Available Relays (Ordered by least load):\n");
    for (uint32_t i = 0; i < relays_amount; i++)
    {
        printf("  [%d] IP: %u.%u.%u.%u | Port: %u | Active Load: %u\n", i,
               relays[i].relay_ip[0], relays[i].relay_ip[1],
               relays[i].relay_ip[2], relays[i].relay_ip[3],
               relays[i].relay_port, relays[i].assigned_count);
    }

    printf("\n[System] Default Route: [%d] -> [%d] -> [%d]\n", 0, 1, 2);
    printf("[System] Press ENTER to accept, or 'c' to build a custom route: ");
    fflush(stdout);

    char choice[INPUT_LINE_SIZE];
    read_line(choice, sizeof(choice));

    relay_decript_t chosen_relays[CIRCUIT_LEN];

    if (choice[0] == 'c' || choice[0] == 'C') 
    {
        printf("[System] Enter 3 relay indices separated by space (e.g. '0 2 1'): ");
        fflush(stdout);
        char custom_indices[INPUT_LINE_SIZE];
        read_line(custom_indices, sizeof(custom_indices));
        
        int i1 = 0, i2 = 0, i3 = 0;
        if (sscanf(custom_indices, "%d %d %d", &i1, &i2, &i3) == 3 &&
            i1 >= 0 && i1 < (int)relays_amount && i2 >= 0 && i2 < (int)relays_amount && i3 >= 0 && i3 < (int)relays_amount) 
        {
            chosen_relays[0] = relays[i1];
            chosen_relays[1] = relays[i2];
            chosen_relays[2] = relays[i3];
            printf("[System] Building custom route...\n");
        } 
        else 
        {
            printf("[System] Invalid input, falling back to default route.\n");
            chosen_relays[0] = relays[0]; chosen_relays[1] = relays[1]; chosen_relays[2] = relays[2];
        }
    } 
    else 
    {
        printf("[System] Building default route...\n");
        chosen_relays[0] = relays[0]; chosen_relays[1] = relays[1]; chosen_relays[2] = relays[2];
    }

    free(relays);

    is_success = build_circuit_with_relays(&new_circuit, chosen_relays);
    if (is_success == false)
    {
        printf("[System] Failed to build circuit.\n> ");
        fflush(stdout);
        return;
    }
    notify_directory_of_route(DIR_SERVER_CFG_PATH, chosen_relays[0].relay_id, chosen_relays[1].relay_id, chosen_relays[2].relay_id);
    
    int rand_res = RAND_bytes(new_e2e_key, E2E_KEY_LEN);
    if (rand_res != OPENSSL_SUCCESS)
    {
        printf("[System] RAND_bytes failed.\n> ");
        fflush(stdout);
        return;
    }

    uint32_t peer_ip_nbo = peer_addr.s_addr;
    uint16_t peer_port_nbo = htons(port);
    
    is_success = chat_send_keyx(&new_circuit, peer_ip_nbo, peer_port_nbo, new_e2e_key, g_my_pub);
    if (is_success == false)
    {
        printf("[System] Failed to send KEYX via circuit.\n> ");
        fflush(stdout);
        return;
    }

    new_index = add_new_connection(new_circuit.guard_fd);
    if (new_index == ERROR_CONNECTION_INDEX)
    {
        printf("[System] Max connections reached. Cannot save circuit.\n> ");
        fflush(stdout);
        return;
    }

    pthread_mutex_lock(&g_conn_mutex);
    
    g_connections[new_index].is_initiator = true;
    g_connections[new_index].circuit = new_circuit;
    g_connections[new_index].peer_ip_nbo = peer_addr.s_addr;
    g_connections[new_index].peer_port_nbo = htons(port);
    
    memcpy(g_connections[new_index].e2e_send_key, new_e2e_key, E2E_KEY_LEN);
    memcpy(g_connections[new_index].e2e_recv_key, new_e2e_key, E2E_KEY_LEN);
    
    g_connections[new_index].is_send_ready = true;
    g_connections[new_index].is_recv_ready = true;
    
    pthread_mutex_unlock(&g_conn_mutex);

    int* thread_arg = malloc(sizeof(int));
    if (thread_arg != NULL)
    {
        *thread_arg = new_index;
        pthread_t worker_thread;
        int create_res = pthread_create(&worker_thread, NULL, worker_read_thread, thread_arg);
        
        if (create_res == 0)
        {
            pthread_detach(worker_thread);
            printf("[System] Connected! Circuit built (Connection ID %d).\n> ", new_index);
            fflush(stdout);
        }
        else
        {
            free(thread_arg);
            close_connection(new_index);
        }
    }
    else
    {
        close_connection(new_index);
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
                bool is_msg_received = tor_recv_msg(current_client->socket_fd, &incoming_msg);
                if (is_msg_received == false)
                {
                    is_client_connected = false;
                }
                else
                {
                    if (current_client->is_initiator == true)
                    {
                        uint8_t plain_data[TOR_MSG_SIZE];
                        uint16_t plain_len = 0;
                        bool is_peeled = recv_onion_data(&current_client->circuit, &incoming_msg, plain_data, &plain_len);
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
        close_connection(connection_index);
        printf("\n[System] Connection %d disconnected.\n> ", connection_index);
        fflush(stdout);
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
                int* thread_arg = malloc(sizeof(int));
                if (thread_arg == NULL)
                {
                    close_connection(new_index);
                }
                else
                {
                    *thread_arg = new_index;
                    pthread_t worker_thread;
                    int create_res = pthread_create(&worker_thread, NULL, worker_read_thread, thread_arg);
                    if (create_res == 0)
                    {
                        pthread_detach(worker_thread);
                    }
                    else
                    {
                        free(thread_arg);
                        close_connection(new_index);
                    }
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
                printf("[Client] Listening on 0.0.0.0:%u\n", port_host);
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
    printf("[System] Chat interface ready.\n");
    
    printf("[System] Commands: %s, %s, %s<IP> <Port>, %s<Conn_ID> <msg>, %s<Chat_ID>, %s\n", 
            CMD_CLIENTS, CMD_CHATS, CMD_CONNECT_PREFIX, CMD_SEND_PREFIX, CMD_HISTORY_PREFIX, CMD_QUIT);
            
    while (is_running == true)
    {
        printf("> ");
        bool has_input = read_line(chat_line, sizeof(chat_line));
        if (has_input == false)
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
                int parsed_items = sscanf(chat_line + CMD_CONNECT_PREFIX_LEN, "%s %d", target_ip, &target_port);
                if (parsed_items == EXPECTED_PARSED_CONNECT)
                {
                    if (target_port >= MIN_PORT_NUMBER && target_port <= MAX_PORT_NUMBER)
                    {
                        initiate_circuit_connection(target_ip, (uint16_t)target_port);
                    }
                }
                else
                {
                    printf("[System] Usage: %s<IP> <Port>\n", CMD_CONNECT_PREFIX);
                }
            }
            else if (strncmp(chat_line, CMD_SEND_PREFIX, CMD_SEND_PREFIX_LEN) == 0)
            {
                int target_id = 0;
                char message_content[CHAT_LINE_SIZE];
                int parsed_items = sscanf(chat_line + CMD_SEND_PREFIX_LEN, "%d %[^\n]", &target_id, message_content);
                if (parsed_items == EXPECTED_PARSED_SEND)
                {
                    bool is_sent = reply_to_client(target_id, message_content);
                    if (is_sent == false)
                    {
                        printf("[System] Failed to send message to connection %d.\n", target_id);
                    }
                }
                else
                {
                    printf("[System] Usage: %s<Connection_ID> <message>\n", CMD_SEND_PREFIX);
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
                    printf("[System] Usage: %s<Chat_ID>\n", CMD_HISTORY_PREFIX);
                }
            }
            else
            {
                if (chat_line[0] != '\0')
                {
                    printf("[System] Unknown command.\n");
                }
            }
        }
    }
}

int start_client_application(void)
{
    int retval = EXIT_SUCCESS;
    char line[INPUT_LINE_SIZE];
    unsigned long local_port = 0;
    int listen_fd = INVALID_SOCKET_FD;
    pthread_t accept_thread_id;
    bool is_success = true;
    
    printf("Enter local listen port: ");
    bool has_input = read_line(line, sizeof(line));
    if (has_input == false)
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
        char db_password[INPUT_LINE_SIZE];
        printf("Enter Master Password to unlock/create your secure vault: ");
        if (read_line(db_password, sizeof(db_password)) == false)
        {
            is_success = false;
            retval = EXIT_FAILURE;
        }
        else
        {
            if (!db_init("/client_data/chat_history.db", db_password))
            {
                printf("[System] Critical Error: Failed to initialize/decrypt the database.\n");
                is_success = false;
                retval = EXIT_FAILURE;
            }
            else
            {
                printf("[System] Database unlocked and ready.\n");
                
                bool found_id = false;
                db_load_identity(g_my_pub, g_my_priv, &found_id);
                if (!found_id) {
                    printf("[System] Generating new permanent Tor Identity...\n");
                    generate_empherial_keypair(g_my_pub, g_my_priv);
                    db_save_identity(g_my_pub, g_my_priv);
                }
                printf("[System] Your Public Identity: %02X%02X%02X%02X...\n", g_my_pub[0], g_my_pub[1], g_my_pub[2], g_my_pub[3]);
            }
        }
    }

    if (is_success == true)
    {
        listen_fd = start_listen_socket((uint16_t)local_port);   
        if (listen_fd < 0)
        {
            is_success = false;
            retval = EXIT_FAILURE;
        }
    }
    if (is_success == true)
    {
        init_connections_manager();
        int thread_create_res = pthread_create(&accept_thread_id, NULL, accept_clients_thread, &listen_fd);
        if (thread_create_res != 0)
        {
            close(listen_fd);
            is_success = false;
            retval = EXIT_FAILURE;
        }
    }
    if (is_success == true)
    {
        run_chat_interface();
        stop_connections_manager();
        shutdown(listen_fd, SHUT_RDWR);
        close(listen_fd);
        pthread_join(accept_thread_id, NULL);
        usleep(100000);
        
        db_close();
    }
    return retval;
}