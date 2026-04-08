#ifndef BRIDGE_PROTOCOL_H
#define BRIDGE_PROTOCOL_H

#include <stdbool.h>
#include <stdint.h>
#include "client_service.h"

#define BRIDGE_LINE_MAX 4096

typedef enum
{
    BRIDGE_CMD_INVALID = 0,
    BRIDGE_CMD_INIT,
    BRIDGE_CMD_CONNECT,
    BRIDGE_CMD_SEND,
    BRIDGE_CMD_PING,
    BRIDGE_CMD_DISCONNECT,
    BRIDGE_CMD_GET_CONNECTIONS,
    BRIDGE_CMD_GET_CHATS,
    BRIDGE_CMD_GET_HISTORY,
    BRIDGE_CMD_SHUTDOWN
} bridge_command_type_e;

typedef struct
{
    bridge_command_type_e type;
    uint16_t listen_port;
    uint16_t port;
    int connection_id;
    int chat_id;
    client_service_route_mode_e route_mode;
    int route_indices[CLIENT_SERVICE_MAX_ROUTE_LEN];
    char ip[64];
    char db_password[256];
    char message[CLIENT_SERVICE_MAX_TEXT_LEN];
} bridge_command_t;

bool bridge_parse_command_line(const char *line, bridge_command_t *out_cmd);

bool bridge_write_ack_json(const char *request_type, bool is_ok, const char *message);

bool bridge_write_event_json(const client_service_event_t *event);

#endif