#define _DEFAULT_SOURCE

#include "client_service.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

static pthread_mutex_t g_service_out_mutex = PTHREAD_MUTEX_INITIALIZER;

static bool read_service_line(char *buf, size_t cap)
{
    bool retval = false;

    if (fgets(buf, cap, stdin) != NULL)
    {
        buf[strcspn(buf, "\n")] = '\0';
        retval = true;
    }

    return retval;
}

static void service_write_line(const char *line)
{
    pthread_mutex_lock(&g_service_out_mutex);
    fputs(line, stdout);
    fputc('\n', stdout);
    fflush(stdout);
    pthread_mutex_unlock(&g_service_out_mutex);
}

static void service_write_ok(const char *command)
{
    char line[128];
    int written = 0;

    memset(line, 0, sizeof(line));
    written = snprintf(line, sizeof(line), "OK|%s", command);

    if (written > 0)
    {
        service_write_line(line);
    }
}

static void service_write_ok_with_id(const char *command, int connection_id)
{
    char line[128];
    int written = 0;

    memset(line, 0, sizeof(line));
    written = snprintf(line, sizeof(line), "OK|%s|%d", command, connection_id);

    if (written > 0)
    {
        service_write_line(line);
    }
}

static void service_write_err(const char *command)
{
    char line[128];
    int written = 0;

    memset(line, 0, sizeof(line));
    written = snprintf(line, sizeof(line), "ERR|%s", command);

    if (written > 0)
    {
        service_write_line(line);
    }
}

static void service_event_callback(const client_core_event_t *event)
{
    char line[CLIENT_SERVICE_LINE_SIZE];
    int written = 0;

    if (event != NULL)
    {
        memset(line, 0, sizeof(line));

        if (event->type == CLIENT_CORE_EVENT_CONNECTED)
        {
            written = snprintf(line,
                               sizeof(line),
                               "EVENT|CONNECTED|%d|%d",
                               event->connection_id,
                               event->chat_id);
        }
        else if (event->type == CLIENT_CORE_EVENT_PING_OK)
        {
            written = snprintf(line,
                               sizeof(line),
                               "EVENT|PING_OK|%d|%d",
                               event->connection_id,
                               event->chat_id);
        }
        else if (event->type == CLIENT_CORE_EVENT_INCOMING_MESSAGE)
        {
            written = snprintf(line,
                               sizeof(line),
                               "EVENT|INCOMING_MESSAGE|%d|%d|%s",
                               event->connection_id,
                               event->chat_id,
                               event->text);
        }
        else if (event->type == CLIENT_CORE_EVENT_CONNECTION_DROPPED)
        {
            written = snprintf(line,
                               sizeof(line),
                               "EVENT|CONNECTION_DROPPED|%d|%d|%s",
                               event->connection_id,
                               event->chat_id,
                               event->text);
        }

        if (written > 0)
        {
            service_write_line(line);
        }
    }
}

static bool handle_init_command(const char *line)
{
    bool retval = false;
    unsigned int port = 0;
    char password[INPUT_LINE_SIZE];

    memset(password, 0, sizeof(password));

    if (sscanf(line, "INIT|%u|%255[^\n]", &port, password) == 2)
    {
        if (port >= MIN_PORT_NUMBER && port <= MAX_PORT_NUMBER)
        {
            retval = client_core_init((uint16_t)port, password);
        }
    }

    return retval;
}

static bool handle_connect_command(const char *line, int *out_connection_id)
{
    bool retval = false;
    char ip[INPUT_LINE_SIZE];
    unsigned int port = 0;

    memset(ip, 0, sizeof(ip));

    if (out_connection_id != NULL)
    {
        *out_connection_id = ERROR_CONNECTION_INDEX;
    }

    if (sscanf(line, "CONNECT|%255[^|]|%u", ip, &port) == 2)
    {
        if (port >= MIN_PORT_NUMBER && port <= MAX_PORT_NUMBER)
        {
            retval = client_core_connect(ip, (uint16_t)port, false, NULL, out_connection_id);
        }
    }

    return retval;
}

static bool handle_connect_custom_command(const char *line, int *out_connection_id)
{
    bool retval = false;
    char ip[INPUT_LINE_SIZE];
    unsigned int port = 0;
    int route_indices[CLIENT_ROUTE_LEN];

    memset(ip, 0, sizeof(ip));
    route_indices[0] = 0;
    route_indices[1] = 0;
    route_indices[2] = 0;

    if (out_connection_id != NULL)
    {
        *out_connection_id = ERROR_CONNECTION_INDEX;
    }

    if (sscanf(line,
               "CONNECT_CUSTOM|%255[^|]|%u|%d|%d|%d",
               ip,
               &port,
               &route_indices[0],
               &route_indices[1],
               &route_indices[2]) == 5)
    {
        if (port >= MIN_PORT_NUMBER && port <= MAX_PORT_NUMBER)
        {
            retval = client_core_connect(ip, (uint16_t)port, true, route_indices, out_connection_id);
        }
    }

    return retval;
}

static bool handle_send_command(const char *line)
{
    bool retval = false;
    const char *first_sep = NULL;
    const char *second_sep = NULL;
    char index_buf[32];
    int connection_index = ERROR_CONNECTION_INDEX;
    char message[CHAT_LINE_SIZE];
    size_t index_len = 0;

    memset(index_buf, 0, sizeof(index_buf));
    memset(message, 0, sizeof(message));

    first_sep = strchr(line, '|');
    if (first_sep != NULL)
    {
        second_sep = strchr(first_sep + 1, '|');
        if (second_sep != NULL)
        {
            index_len = (size_t)(second_sep - (first_sep + 1));
            if (index_len < sizeof(index_buf))
            {
                memcpy(index_buf, first_sep + 1, index_len);
                connection_index = atoi(index_buf);
                strncpy(message, second_sep + 1, sizeof(message) - 1);
                retval = client_core_send(connection_index, message);
            }
        }
    }

    return retval;
}

static bool handle_ping_command(const char *line)
{
    bool retval = false;
    int connection_index = ERROR_CONNECTION_INDEX;

    if (sscanf(line, "PING|%d", &connection_index) == 1)
    {
        retval = client_core_ping(connection_index);
    }

    return retval;
}

static bool handle_disconnect_command(const char *line)
{
    bool retval = false;
    int connection_index = ERROR_CONNECTION_INDEX;

    if (sscanf(line, "DISCONNECT|%d", &connection_index) == 1)
    {
        retval = client_core_disconnect(connection_index);
    }

    return retval;
}

int run_client_service(void)
{
    int retval = EXIT_SUCCESS;
    bool is_running = true;
    char line[CLIENT_SERVICE_LINE_SIZE];

    client_core_set_service_mode(true);
    client_core_set_event_callback(service_event_callback);

    while (is_running == true)
    {
        if (read_service_line(line, sizeof(line)) == false)
        {
            is_running = false;
        }

        if (is_running == true)
        {
            if (strncmp(line, "INIT|", 5) == 0)
            {
                if (handle_init_command(line) == true)
                {
                    service_write_ok("INIT");
                }
                else
                {
                    service_write_err("INIT");
                }
            }
            else if (strncmp(line, "CONNECT_CUSTOM|", 15) == 0)
            {
                int connection_id = ERROR_CONNECTION_INDEX;

                if (handle_connect_custom_command(line, &connection_id) == true)
                {
                    service_write_ok_with_id("CONNECT_CUSTOM", connection_id);
                }
                else
                {
                    service_write_err("CONNECT_CUSTOM");
                }
            }
            else if (strncmp(line, "CONNECT|", 8) == 0)
            {
                int connection_id = ERROR_CONNECTION_INDEX;

                if (handle_connect_command(line, &connection_id) == true)
                {
                    service_write_ok_with_id("CONNECT", connection_id);
                }
                else
                {
                    service_write_err("CONNECT");
                }
            }
            else if (strncmp(line, "SEND|", 5) == 0)
            {
                if (handle_send_command(line) == true)
                {
                    service_write_ok("SEND");
                }
                else
                {
                    service_write_err("SEND");
                }
            }
            else if (strncmp(line, "PING|", 5) == 0)
            {
                if (handle_ping_command(line) == true)
                {
                    service_write_ok("PING");
                }
                else
                {
                    service_write_err("PING");
                }
            }
            else if (strncmp(line, "DISCONNECT|", 11) == 0)
            {
                if (handle_disconnect_command(line) == true)
                {
                    service_write_ok("DISCONNECT");
                }
                else
                {
                    service_write_err("DISCONNECT");
                }
            }
            else if (strcmp(line, "LIST") == 0)
            {
                client_core_list_connections();
                service_write_ok("LIST");
            }
            else if (strcmp(line, "QUIT") == 0)
            {
                client_core_shutdown();
                service_write_ok("QUIT");
                is_running = false;
            }
            else
            {
                service_write_err("UNKNOWN_COMMAND");
            }
        }
    }

    return retval;
}