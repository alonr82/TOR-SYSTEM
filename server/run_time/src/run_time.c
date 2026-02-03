#include "run_time.h"

static int server_socket_fd;

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

static bool handle_send_relay(int client_fd)
{
    bool retval =true;
    uint32_t start = 0;
    relay_decript_t *relay_list = calloc(MAX_RELAY_BATCH_SIZE, sizeof(relay_decript_t));
    if(relay_list == NULL)
    {
        retval = false;
    }
    else
    {
        uint32_t fetched_relays = get_relay_batch(relay_list, &start, MAX_RELAY_BATCH_SIZE);
        for(uint32_t index = 0; index < fetched_relays; index++)
        {
            printf("Relay index: %u, IP adress: %u.%u.%u.%u, Port: %u, Active: %u\n", index,
                   relay_list[index].relay_ip[0],
                   relay_list[index].relay_ip[1],
                   relay_list[index].relay_ip[2],
                   relay_list[index].relay_ip[3],
                   relay_list[index].relay_port,
                   relay_list[index].is_active);
        }
        if(write_exact(client_fd,&fetched_relays,sizeof(uint32_t)) == false)
        {
            retval = false;
        }
        else
        {
            if(write_exact(client_fd, relay_list, fetched_relays * sizeof(relay_decript_t)) == false)
            {
                retval = false;
            }
        }
        free(relay_list);
    }
    return retval;

}

relay_decript_t* get_data_from_req(user_descriptor_t* user, relay_req_t* request)
{
    relay_decript_t* retval = calloc(1, sizeof(relay_decript_t));
    if(retval != NULL)
    {
        struct sockaddr_in* addr_in = (struct sockaddr_in*)&user->addr;
        retval->ip_type = 4;
        memcpy(retval->relay_ip, &addr_in->sin_addr, IP4_SIZE);
        retval->relay_port = request->request_details_u.signup_request.relay_port;
        retval->is_active = true;
    }
    return retval;
}

static void client_callback(user_descriptor_t* user)
{
    relay_req_res_t retval = {0};
    request_t request = {0};
    if (read_exact(user->fd, &request, sizeof(request_t)) == false)
    {
        fprintf(stderr, "Failed to receive full request\n");
        close(user->fd);
        retval.responese_details_u.signup_response.status = false;
        retval.responese_details_u.signout_response.status = false;
    }
    else
    {
        if(request.request_type == RELAY_REQUEST && request.request_u.relay_req.request_type == RELAY_REG_SIGNUP)
        {
            relay_decript_t* relay_data = get_data_from_req(user,&request.request_u.relay_req);
            relay_data_t* new_relay = generate_relay(relay_data);
            printf("Relay data: IP address: %u.%u.%u.%u, Port: %u\n",
                   relay_data->relay_ip[0],
                   relay_data->relay_ip[1],
                   relay_data->relay_ip[2],
                   relay_data->relay_ip[3],
                   relay_data->relay_port);
            free(relay_data);
            if (new_relay == NULL)
            {
                fprintf(stderr, "Failed to generate new relay\n");
                close(user->fd);
                retval.responese_details_u.signup_response.status = false;
            }
            else
            {
                retval.responese_details_u.signup_response.relay_id = new_relay->relay_id;
                retval.responese_details_u.signup_response.status = true;
                if (write_exact(user->fd, &retval, sizeof(relay_req_res_t)) == false)
                {
                    fprintf(stderr, "Failed to send full response\n");
                    close(user->fd);
                    retval.responese_details_u.signup_response.status = false;
                }
                else
                {
                    printf("Registered new relay with ID: %u\n", new_relay->relay_id);
                    close(user->fd);
                }
            }
            
        }
        else if(request.request_type == RELAY_REQUEST && request.request_u.relay_req.request_type == RELAY_REG_SIGNOUT)
        {
            bool ret_value_remove = remove_relay(request.request_u.relay_req.request_details_u.signout_request.relay_id);
            retval.responese_details_u.signout_response.status = ret_value_remove;
            if (write_exact(user->fd, &retval, sizeof(relay_req_res_t)) == false)
            {
                fprintf(stderr, "Failed to send full response\n");
                close(user->fd);
                retval.responese_details_u.signout_response.status = false;
            }
            else
            {
                printf("Removed relay with ID: %u\n", request.request_u.relay_req.request_details_u.signout_request.relay_id);
                close(user->fd);
            }
        }
        else if(request.request_type == CLIENT_REQUEST)
        {
            handle_send_relay(user->fd);
        }
    }
    free(user);
    return;
}

static void* relay_accept_loop_func(void* _)
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
            pthread_t connection_thread_id;
            int con_thread_error = pthread_create(&connection_thread_id, NULL, relay_accept_loop_func, NULL);
            if (con_thread_error)
            {
                retval = running_status_failure;
            }
            else
            {
                run_commands();
                shutdown(server_socket_fd, SHUT_RDWR);
                close(server_socket_fd);
                server_socket_fd = FAILURE;
                pthread_join(connection_thread_id, NULL);
                free_relay_manager();
            }

        }
    }
    if (server_socket_fd != FAILURE)
    {
        close(server_socket_fd);
        server_socket_fd = FAILURE;
    }
    free_server_config(config);
    return retval;

}
