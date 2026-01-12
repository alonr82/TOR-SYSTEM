#include "relay_run_time.h"

static int relay_listen_fd;

static void relay_client_callback(user_descriptor_t* user)
{
    tor_msg_t message;
    if(tor_recv_msg(user->fd, &message) != true)
    {
        printf("relay_run_time: failed to recieve message from client\n");
        close(user->fd);
        free(user);
        return;
    }
    else
    {
        if(message.header.type == TOR_MSG_EXTEND)
        {
            printf("relay_run_time: recieved EXTEND message from client\n");
        }
        else if(message.header.type == TOR_MSG_DATA)
        {
            printf("relay_run_time: recieved DATA message from client\n");
        }
        else
        {
            printf("relay_run_time: recieved UNKNOWN message type from client\n");
        }
        close(user->fd);
        free(user);
        return;
    }
}

static void* relay_accept_loop_func(void* _)
{   
    (void)(_);

    accept_loop(relay_listen_fd, relay_client_callback);

    return NULL;
}


bool run_relay(const char * dir_cfg_path)
{
    bool retval = true;
    relay_req_res_t * signup_response = NULL;
    signup_response = relay_connect_only(dir_cfg_path, &relay_listen_fd);
    if(signup_response == NULL)
    {
        retval = false;
        printf("relay_run_time: failed to sign up relay with directory server\n");
    }
    else
    {
        pthread_t accept_thread;
        if(pthread_create(&accept_thread, NULL, relay_accept_loop_func, NULL) != SUCCESS)
        {
            printf("relay_run_time: failed to create accept loop thread\n");
            retval = false;
        }
        else
        {
            printf("relay_run_time: relay is running and accepting connections\n");
            pthread_join(accept_thread, NULL);
        }
        free(signup_response);
    }
    return retval;

}
