#ifndef CLIENT_RUN_H
#define CLIENT_RUN_H

#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>
#include "tor_protocol.h"
#include "create_circuit.h"

#define MAX_ACTIVE_CONNECTIONS  10
#define E2E_KEY_LEN 32
#define INVALID_SOCKET_FD -1
#define ERROR_CONNECTION_INDEX -1
#define BASE_DECIMAL 10
#define MAX_PORT_NUMBER 65535
#define MIN_PORT_NUMBER 1

#define MAGIC_KEYX_STR "KEYX"
#define MAGIC_KEYX_LEN 4
#define INPUT_LINE_SIZE 256
#define CHAT_LINE_SIZE 1024
#define LISTEN_BACKLOG 16
#define DIR_SERVER_CFG_PATH "config/dir_server_config.cfg"

#define CMD_QUIT "/quit"
#define CMD_CLIENTS "/clients"
#define CMD_CHATS "/chats"
#define CMD_HISTORY_PREFIX "/history "
#define CMD_HISTORY_PREFIX_LEN 9
#define CMD_SEND_PREFIX "/send "
#define CMD_SEND_PREFIX_LEN 6
#define CMD_PING_PREFIX "/ping "
#define CMD_PING_PREFIX_LEN 6
#define CMD_CONNECT_PREFIX "/connect "
#define CMD_CONNECT_PREFIX_LEN  9

#define EXPECTED_PARSED_SEND 2
#define EXPECTED_PARSED_CONNECT 2
#define OPENSSL_SUCCESS 1
#define CLIENT_ROUTE_LEN 3

typedef struct 
{
    int socket_fd;
    bool is_active;
    bool is_initiator;
    circuit_t circuit;
    uint32_t peer_ip_nbo;
    uint16_t peer_port_nbo;
    bool is_recv_ready;
    uint8_t e2e_recv_key[E2E_KEY_LEN];
    bool is_send_ready;
    uint8_t e2e_send_key[E2E_KEY_LEN];
    int peer_db_id; 
} connection_context_t;

typedef enum
{
    CLIENT_CORE_EVENT_NONE = 0,
    CLIENT_CORE_EVENT_CONNECTED,
    CLIENT_CORE_EVENT_PING_OK,
    CLIENT_CORE_EVENT_INCOMING_MESSAGE,
    CLIENT_CORE_EVENT_CONNECTION_DROPPED
} client_core_event_type_e;

typedef struct
{
    client_core_event_type_e type;
    int connection_id;
    int chat_id;
    char text[CHAT_LINE_SIZE];
} client_core_event_t;

typedef void (*client_core_event_callback_t)(const client_core_event_t *event);

int start_client_application(void);

void client_core_set_service_mode(bool is_service_mode);

void client_core_set_event_callback(client_core_event_callback_t callback);

bool client_core_init(uint16_t local_port, const char *db_password);

void client_core_shutdown(void);

bool client_core_connect(const char *ip_str,
                         uint16_t port,
                         bool use_custom_route,
                         const int route_indices[CLIENT_ROUTE_LEN],
                         int *out_connection_id);

bool client_core_send(int connection_index, const char *message);

bool client_core_ping(int connection_index);

bool client_core_disconnect(int connection_index);

void client_core_list_connections(void);

int run_client_service(void);

#endif