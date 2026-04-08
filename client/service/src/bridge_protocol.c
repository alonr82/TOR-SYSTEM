#include "bridge_protocol.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static void trim_newline(char *line)
{
    if (line != NULL)
    {
        line[strcspn(line, "\r\n")] = '\0';
    }
}

static void json_escape_and_write(const char *text)
{
    if (text != NULL)
    {
        for (size_t index = 0; text[index] != '\0'; index++)
        {
            if (text[index] == '\\')
            {
                fputs("\\\\", stdout);
            }
            else if (text[index] == '\"')
            {
                fputs("\\\"", stdout);
            }
            else if (text[index] == '\n')
            {
                fputs("\\n", stdout);
            }
            else if (text[index] == '\r')
            {
                fputs("\\r", stdout);
            }
            else if (text[index] == '\t')
            {
                fputs("\\t", stdout);
            }
            else
            {
                fputc(text[index], stdout);
            }
        }
    }
}

static bool parse_init_command(char *line_copy, bridge_command_t *out_cmd)
{
    bool retval = false;
    char *save_ptr = NULL;
    char *token = strtok_r(line_copy, "|", &save_ptr);
    char *port_token = NULL;
    char *password_token = NULL;

    if (token != NULL)
    {
        port_token = strtok_r(NULL, "|", &save_ptr);
        password_token = strtok_r(NULL, "", &save_ptr);

        if (port_token != NULL && password_token != NULL)
        {
            out_cmd->type = BRIDGE_CMD_INIT;
            out_cmd->listen_port = (uint16_t)strtoul(port_token, NULL, BASE_DECIMAL);
            strncpy(out_cmd->db_password, password_token, sizeof(out_cmd->db_password) - 1);
            retval = true;
        }
    }

    return retval;
}

static bool parse_connect_command(char *line_copy, bridge_command_t *out_cmd)
{
    bool retval = false;
    char *save_ptr = NULL;
    char *token = strtok_r(line_copy, "|", &save_ptr);
    char *ip_token = NULL;
    char *port_token = NULL;
    char *mode_token = NULL;
    char *r1_token = NULL;
    char *r2_token = NULL;
    char *r3_token = NULL;

    if (token != NULL)
    {
        ip_token = strtok_r(NULL, "|", &save_ptr);
        port_token = strtok_r(NULL, "|", &save_ptr);
        mode_token = strtok_r(NULL, "|", &save_ptr);

        if (ip_token != NULL && port_token != NULL && mode_token != NULL)
        {
            out_cmd->type = BRIDGE_CMD_CONNECT;
            strncpy(out_cmd->ip, ip_token, sizeof(out_cmd->ip) - 1);
            out_cmd->port = (uint16_t)strtoul(port_token, NULL, BASE_DECIMAL);

            if (strcmp(mode_token, "default") == 0)
            {
                out_cmd->route_mode = CLIENT_SERVICE_ROUTE_DEFAULT;
                retval = true;
            }
            else if (strcmp(mode_token, "custom") == 0)
            {
                r1_token = strtok_r(NULL, "|", &save_ptr);
                r2_token = strtok_r(NULL, "|", &save_ptr);
                r3_token = strtok_r(NULL, "|", &save_ptr);

                if (r1_token != NULL && r2_token != NULL && r3_token != NULL)
                {
                    out_cmd->route_mode = CLIENT_SERVICE_ROUTE_CUSTOM;
                    out_cmd->route_indices[0] = atoi(r1_token);
                    out_cmd->route_indices[1] = atoi(r2_token);
                    out_cmd->route_indices[2] = atoi(r3_token);
                    retval = true;
                }
            }
        }
    }

    return retval;
}

static bool parse_send_command(const char *line, bridge_command_t *out_cmd)
{
    bool retval = false;
    const char *first_delim = strchr(line, '|');
    const char *second_delim = NULL;

    if (first_delim != NULL)
    {
        second_delim = strchr(first_delim + 1, '|');
        if (second_delim != NULL)
        {
            size_t conn_len = (size_t)(second_delim - (first_delim + 1));
            char conn_buf[32];

            if (conn_len < sizeof(conn_buf))
            {
                memset(conn_buf, 0, sizeof(conn_buf));
                memcpy(conn_buf, first_delim + 1, conn_len);

                out_cmd->type = BRIDGE_CMD_SEND;
                out_cmd->connection_id = atoi(conn_buf);
                strncpy(out_cmd->message, second_delim + 1, sizeof(out_cmd->message) - 1);
                retval = true;
            }
        }
    }

    return retval;
}

static bool parse_single_int_command(char *line_copy,
                                     bridge_command_t *out_cmd,
                                     bridge_command_type_e command_type,
                                     int *out_value)
{
    bool retval = false;
    char *save_ptr = NULL;
    char *token = strtok_r(line_copy, "|", &save_ptr);
    char *value_token = NULL;

    if (token != NULL)
    {
        value_token = strtok_r(NULL, "|", &save_ptr);
        if (value_token != NULL)
        {
            out_cmd->type = command_type;
            *out_value = atoi(value_token);
            retval = true;
        }
    }

    return retval;
}

bool bridge_parse_command_line(const char *line, bridge_command_t *out_cmd)
{
    bool retval = false;
    char line_copy[BRIDGE_LINE_MAX];

    if (line != NULL && out_cmd != NULL)
    {
        memset(out_cmd, 0, sizeof(*out_cmd));
        memset(line_copy, 0, sizeof(line_copy));
        strncpy(line_copy, line, sizeof(line_copy) - 1);
        trim_newline(line_copy);

        if (strncmp(line_copy, "INIT|", 5) == 0)
        {
            retval = parse_init_command(line_copy, out_cmd);
        }
        else if (strncmp(line_copy, "CONNECT|", 8) == 0)
        {
            retval = parse_connect_command(line_copy, out_cmd);
        }
        else if (strncmp(line_copy, "SEND|", 5) == 0)
        {
            retval = parse_send_command(line_copy, out_cmd);
        }
        else if (strncmp(line_copy, "PING|", 5) == 0)
        {
            retval = parse_single_int_command(line_copy, out_cmd, BRIDGE_CMD_PING, &out_cmd->connection_id);
        }
        else if (strncmp(line_copy, "DISCONNECT|", 11) == 0)
        {
            retval = parse_single_int_command(line_copy, out_cmd, BRIDGE_CMD_DISCONNECT, &out_cmd->connection_id);
        }
        else if (strcmp(line_copy, "GET_CONNECTIONS") == 0)
        {
            out_cmd->type = BRIDGE_CMD_GET_CONNECTIONS;
            retval = true;
        }
        else if (strcmp(line_copy, "GET_CHATS") == 0)
        {
            out_cmd->type = BRIDGE_CMD_GET_CHATS;
            retval = true;
        }
        else if (strncmp(line_copy, "GET_HISTORY|", 12) == 0)
        {
            retval = parse_single_int_command(line_copy, out_cmd, BRIDGE_CMD_GET_HISTORY, &out_cmd->chat_id);
        }
        else if (strcmp(line_copy, "SHUTDOWN") == 0)
        {
            out_cmd->type = BRIDGE_CMD_SHUTDOWN;
            retval = true;
        }
    }

    return retval;
}

bool bridge_write_ack_json(const char *request_type, bool is_ok, const char *message)
{
    bool retval = true;

    fputs("{\"type\":\"ack\",\"requestType\":\"", stdout);
    json_escape_and_write(request_type);
    fputs("\",\"ok\":", stdout);

    if (is_ok == true)
    {
        fputs("true", stdout);
    }
    else
    {
        fputs("false", stdout);
    }

    fputs(",\"message\":\"", stdout);
    json_escape_and_write(message);
    fputs("\"}\n", stdout);
    fflush(stdout);

    return retval;
}

bool bridge_write_event_json(const client_service_event_t *event)
{
    bool retval = true;

    if (event == NULL)
    {
        retval = false;
    }
    else
    {
        if (event->type == CLIENT_SERVICE_EVENT_INIT_OK)
        {
            fputs("{\"type\":\"init_ok\",\"identity\":\"", stdout);
            json_escape_and_write(event->text);
            fputs("\"}\n", stdout);
        }
        else if (event->type == CLIENT_SERVICE_EVENT_STATUS)
        {
            fputs("{\"type\":\"status\",\"value\":\"", stdout);
            json_escape_and_write(event->text);
            fputs("\"}\n", stdout);
        }
        else if (event->type == CLIENT_SERVICE_EVENT_CONNECTED)
        {
            fprintf(stdout,
                    "{\"type\":\"connected\",\"connectionId\":%d,\"chatId\":%d}\n",
                    event->connection_id,
                    event->chat_id);
        }
        else if (event->type == CLIENT_SERVICE_EVENT_SENT_SECURELY)
        {
            fprintf(stdout,
                    "{\"type\":\"sent_securely\",\"connectionId\":%d}\n",
                    event->connection_id);
        }
        else if (event->type == CLIENT_SERVICE_EVENT_INCOMING_MESSAGE)
        {
            fprintf(stdout,
                    "{\"type\":\"incoming_message\",\"chatId\":%d,\"body\":\"",
                    event->chat_id);
            json_escape_and_write(event->text);
            fputs("\"}\n", stdout);
        }
        else if (event->type == CLIENT_SERVICE_EVENT_CONNECTION_DROPPED)
        {
            fprintf(stdout,
                    "{\"type\":\"connection_dropped\",\"connectionId\":%d,\"reason\":\"",
                    event->connection_id);
            json_escape_and_write(event->text);
            fputs("\"}\n", stdout);
        }
        else if (event->type == CLIENT_SERVICE_EVENT_CONNECTION_INFO)
        {
            fprintf(stdout,
                    "{\"type\":\"connection_info\",\"connectionId\":%d,\"chatId\":%d,\"status\":\"",
                    event->connection_id,
                    event->chat_id);
            json_escape_and_write(event->text);
            fputs("\"}\n", stdout);
        }
        else if (event->type == CLIENT_SERVICE_EVENT_ERROR)
        {
            fputs("{\"type\":\"error\",\"message\":\"", stdout);
            json_escape_and_write(event->text);
            fputs("\"}\n", stdout);
        }
        else
        {
            retval = false;
        }

        if (retval == true)
        {
            fflush(stdout);
        }
    }

    return retval;
}