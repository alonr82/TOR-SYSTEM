#include "run_time.h"
#include "relay_manager.h"
#include "sock_utilities.h"
#include <pthread.h>
static uint16_t server_socket_fd;

static void run_commands()
{
    bool running = true;
    while (running)
    {
        char input[INPUT_SIZE];
        if (fgets(input, sizeof(input), stdin) != input)
        {
            fprintf(stderr, "Invalid input");
            break;
        }
        input[strcspn(input, "\n")] = '\0';
        printf("`%s`\n", input);
        if (strcmp(input, "exit") == 0)
        {
            running = false;
        }
    }
}

static void client_callback(user_descriptor_t* user)
{
    msg_server_buffer_t buffer_data;
    uint32_t id = user->fd;
    process_relay(user->fd, &buffer_data);
    free(user);
    return;
}

static void* accept_loop_func(void* _)
{   
    (void)(_);

    accept_loop(server_socket_fd, client_callback);

    return NULL;
}

server_running_status_e run_dir_server(const char *config_file)
{
    server_running_status_e retval = running_status_success;
    server_config_metadata_t* config;
    config = fetch_server_config((char*)config_file);
    if(config == NULL)
    {
        fprintf(stderr, "Failed to do configuration for the server\n");
        retval = running_status_failure;
    }
    else
    {
        init_relay_manager();
        server_socket_fd = create_and_bind(config);
        if(server_socket_fd == -1)
        {
            fprintf(stderr, "Failed to bind \n");
            retval = running_status_failure;
        }
        else
        {
            if(listen(server_socket_fd,10) < 0)
            {
                perror("listen");
                close(server_socket_fd);
                retval = running_status_failure;
            }
            else
            {
                pthread_t connection_thread_id;
                int con_thread_error = pthread_create(&connection_thread_id,NULL,accept_loop_func,NULL);
                if(con_thread_error)
                {
                    retval = running_status_failure;
                }
                else
                {
                    pthread_detach(connection_thread_id);
                }
                run_commands();
                free_relay_manager();
            }
        }
    }
    close(server_socket_fd);
    return retval;
}
