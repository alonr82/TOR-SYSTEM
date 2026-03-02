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
#define CMD_SEND_PREFIX "/send "
#define CMD_SEND_PREFIX_LEN 6
#define CMD_CONNECT_PREFIX "/connect "
#define CMD_CONNECT_PREFIX_LEN  9

#define EXPECTED_PARSED_SEND 2
#define EXPECTED_PARSED_CONNECT 2
#define OPENSSL_SUCCESS 1

typedef struct {
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
} connection_context_t;

int start_client_application(void);

#endif 