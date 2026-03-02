#define _POSIX_C_SOURCE 200112L

#include "client_run.h"
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

static void init_connections_manager(void)
{
    for (int index = 0; index < MAX_ACTIVE_CONNECTIONS; index++)
    {
        g_connections[index].socket_fd = INVALID_SOCKET_FD;
        g_connections[index].is_active = false;
        g_connections[index].is_initiator = false;
        g_connections[index].is_recv_ready = false;
        g_connections[index].is_send_ready = false;
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
    
    printf("\n--- Active Clients ---\n");
    for (int i = 0; i < MAX_ACTIVE_CONNECTIONS; i++)
    {
        if (g_connections[i].is_active == true)
        {
            if (g_connections[i].is_initiator == true)
            {
                printf("Client ID: %d | Guard FD: %d (Outgoing Tor Circuit)\n", i, g_connections[i].socket_fd);
            }
            else
            {
                printf("Client ID: %d | Socket FD: %d (Incoming Connection)\n", i, g_connections[i].socket_fd);
            }
        }
    }
    printf("----------------------\n> ");
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
        }
    }
    return retval;
}

static void handle_client_message(connection_context_t* client, tor_msg_t* msg)
{
    if (msg->header.type == TOR_MSG_DATA)
    {
        uint16_t payload_len = ntohs(msg->header.payload_len);
        uint16_t expected_keyx_len = (uint16_t)(MAGIC_KEYX_LEN + E2E_KEY_LEN);

        if (payload_len == expected_keyx_len)
        {
            int is_keyx = memcmp(msg->payload, MAGIC_KEYX_STR, MAGIC_KEYX_LEN);
            if (is_keyx == 0)
            {
                memcpy(client->e2e_recv_key, msg->payload + MAGIC_KEYX_LEN, E2E_KEY_LEN);
                client->is_recv_ready = true;
                memcpy(client->e2e_send_key, client->e2e_recv_key, E2E_KEY_LEN);
                client->is_send_ready = true;
                
                printf("\n[Client on FD %d] Handshake completed (KEYX received).\n> ", client->socket_fd);
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
                        printf("\n[Peer] %s\n> ", (char*)plaintext);
                        fflush(stdout);
                    }
                }
            }
        }
    }
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
        is_success = false;
        printf("[System] Invalid IP address.\n> ");
        fflush(stdout);
    }

    if (is_success == true)
    {
        memset(&new_circuit, 0, sizeof(new_circuit));
        printf("[System] Building Tor circuit...\n");
        is_success = build_circuit_from_dir(DIR_SERVER_CFG_PATH, &new_circuit);
        
        if (is_success == false)
        {
            printf("[System] Failed to build circuit.\n> ");
            fflush(stdout);
        }
    }

    if (is_success == true)
    {
        int rand_res = RAND_bytes(new_e2e_key, E2E_KEY_LEN);
        if (rand_res != OPENSSL_SUCCESS)
        {
            is_success = false;
            printf("[System] RAND_bytes failed.\n> ");
            fflush(stdout);
        }
    }

    if (is_success == true)
    {
        uint32_t peer_ip_nbo = peer_addr.s_addr;
        uint16_t peer_port_nbo = htons(port);
        
        is_success = chat_send_keyx(&new_circuit, peer_ip_nbo, peer_port_nbo, new_e2e_key);
        if (is_success == false)
        {
            printf("[System] Failed to send KEYX via circuit.\n> ");
            fflush(stdout);
        }
    }

    if (is_success == true)
    {
        new_index = add_new_connection(new_circuit.guard_fd);
        if (new_index == ERROR_CONNECTION_INDEX)
        {
            is_success = false;
            printf("[System] Max connections reached. Cannot save circuit.\n> ");
            fflush(stdout);
        }
    }

    if (is_success == true)
    {
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
                printf("[System] Connected! Circuit built (Client ID %d).\n> ", new_index);
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
                            handle_client_message(current_client, peeled_msg);
                        }
                    }
                    else
                    {
                        handle_client_message(current_client, &incoming_msg);
                    }
                }
            }
        }

        close_connection(connection_index);
        printf("\n[System] Client %d disconnected.\n> ", connection_index);
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
    printf("[System] Commands: %s, %s<IP> <Port>, %s<id> <msg>, %s\n", CMD_CLIENTS, CMD_CONNECT_PREFIX, CMD_SEND_PREFIX, CMD_QUIT);

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
            else if (strncmp(chat_line, CMD_CONNECT_PREFIX, CMD_CONNECT_PREFIX_LEN) == 0)
            {
                char target_ip[INPUT_LINE_SIZE];
                int target_port = 0;
                int parsed_items = sscanf(chat_line + CMD_CONNECT_PREFIX_LEN, "%s %d", target_ip, &target_port);

                if (parsed_items == EXPECTED_PARSED_CONNECT)
                {
                    if (target_port >= MIN_PORT_NUMBER)
                    {
                        if (target_port <= MAX_PORT_NUMBER)
                        {
                            initiate_circuit_connection(target_ip, (uint16_t)target_port);
                        }
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
                        printf("[System] Failed to send message to client %d.\n", target_id);
                    }
                }
                else
                {
                    printf("[System] Usage: %s<id> <message>\n", CMD_SEND_PREFIX);
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
        
        if (local_port < MIN_PORT_NUMBER)
        {
            is_success = false;
            retval = EXIT_FAILURE;
        }
        
        if (local_port > MAX_PORT_NUMBER)
        {
            is_success = false;
            retval = EXIT_FAILURE;
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
    }

    return retval;
}