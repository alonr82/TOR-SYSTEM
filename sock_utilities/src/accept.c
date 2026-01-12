#define _POSIX_C_SOURCE 200112L
#include "sock_utilities.h"

static void (*g_connection_handler)(user_descriptor_t* user) = NULL;

static void* connection_handler_wrapper(void* arg)
{
    user_descriptor_t* user = (user_descriptor_t*)arg;
    if (g_connection_handler != NULL)
    {
        g_connection_handler(user);
    }
    return NULL;
}



void accept_loop(int server_fd, void (*connection_handler)(user_descriptor_t* user))
{
    g_connection_handler = connection_handler;
    while (true)
    {
        struct sockaddr_storage client_addr;
        socklen_t addr_len = sizeof(client_addr);
        int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &addr_len);
        if (client_fd < 0)
        {
            break;;
        }
        user_descriptor_t* user_desc = malloc(sizeof(user_descriptor_t)); 
        user_desc->fd = client_fd;
        user_desc->addr = client_addr;
        user_desc->addr_len = addr_len;
        pthread_t thread_id;
        if (pthread_create(&thread_id, NULL, connection_handler_wrapper, (void*)user_desc) != 0)
        {
            close(client_fd);
            free(user_desc);
            continue;
        }
        pthread_detach(thread_id);
    }
}

void set_socket_nonblock(int sock_fd) 
{
    int flags = fcntl(sock_fd, F_GETFL, 0);
    fcntl(sock_fd, F_SETFL, flags | O_NONBLOCK);
}