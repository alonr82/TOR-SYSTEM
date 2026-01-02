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

relay_decript_t* get_data_from_req(user_descriptor_t* user, relay_request_t* request)
{
    relay_decript_t* retval = calloc(1, sizeof(relay_decript_t));
    if(retval != NULL)
    {
        struct sockaddr_in* addr_in = (struct sockaddr_in*)&user->addr;
        retval->ip_type = 4;
        memcpy(retval->relay_ip, &addr_in->sin_addr, IP4_SIZE);
        retval->relay_port = request->relay_port;
        retval->is_active = true;
    }
    return retval;
}

static bool client_callback(user_descriptor_t* user)
{
    bool retval = true;
    relay_request_t request;
    ssize_t recv_bytes = recv(user->fd, &request, sizeof(request), 0);
    if (recv_bytes != sizeof(request))
    {
        fprintf(stderr, "Failed to receive full request\n");
        close(user->fd);
        retval = false;
    }
    else
    {
        if(request.request_type == RELAY_REG_SIGNUP)
        {
            relay_decript_t* relay_data = get_data_from_req(user,&request);
            relay_data_t* new_relay = generate_relay(relay_data);
            free(relay_data);
            if (new_relay == NULL)
            {
                fprintf(stderr, "Failed to generate new relay\n");
                close(user->fd);
                retval = false;
            }
            else
            {
                relay_req_response_t response;
                response.relay_id = new_relay->relay_id;
                response.status = true;
                ssize_t sent_bytes = send(user->fd, &response, sizeof(response), 0);
                if (sent_bytes != sizeof(response))
                {
                    fprintf(stderr, "Failed to send full response\n");
                    close(user->fd);
                    retval = false;
                }
                else
                {
                    printf("Registered new relay with ID: %u\n", new_relay->relay_id);
                    close(user->fd);
                }
            }
        }
        else if(request.request_type == RELAY_REG_SIGNOUT)
        {
            // Handle signout request if needed
            close(user->fd);
        }
    }
    free(user);
    return retval;
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
    free_server_config(config);
    return retval;
}
