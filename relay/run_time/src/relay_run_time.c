#define _DEFAULT_SOURCE 

#include "relay_run_time.h"
#include <unistd.h> 

static int relay_listen_fd = -1;
static relay_req_res_t* relay_signup_response;
static volatile bool relay_running = true;
static pthread_t accept_thread;

uint8_t g_relay_identity_pub[TOR_ED25519_PUB_LEN];
uint8_t g_relay_identity_priv_seed[TOR_ED25519_SEED_LEN];

static void relay_run_commands(void)
{
    char input[128];

    while (relay_running)
    {
        struct timeval tv = {0, 500000}; 
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(STDIN_FILENO, &fds);

        int ret = select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv);
        
        if (ret > 0)
        {
            if (fgets(input, sizeof(input), stdin) == NULL) break;
            input[strcspn(input, "\n")] = '\0';

            if (strcmp(input, "exit") == 0)
            {
                printf("relay: shutting down...\n");
                relay_running = false;
                
                if (relay_listen_fd >= 0)
                {
                    close(relay_listen_fd);
                    relay_listen_fd = -1;
                }
                break;
            }
        }
    }
}

static void relay_client_callback(user_descriptor_t* user)
{
    tor_msg_t message;
    
    printf("\n>>> RELAY: Accepted new connection on FD %d\n", user->fd);

    if(!relay_running) 
    {
        printf("RELAY: Shutting down, dropping connection.\n");
        close(user->fd);
        return;
    }

    if(tor_recv_msg(user->fd, &message) != true)
    {
        printf("RELAY: tor_recv_msg failed! Closing connection on FD %d\n", user->fd);
        close(user->fd);
        return;
    }
    
    printf("RELAY: Successfully received message type: %d\n", message.header.type);
    
    if(message.header.type == TOR_MSG_CREATE)
    {
        session_t session;
        memset(&session, 0, sizeof(session_t));
        session.last_fd = user->fd;
        session.next_fd = -1;
        
        tor_msg_t reply_msg;
        memset(&reply_msg, 0, sizeof(reply_msg));
        
        if(process_create_handshake(&session, &message, &reply_msg, g_relay_identity_priv_seed))
        {
            if(tor_send_msg(session.last_fd, &reply_msg))
            {
                printf("relay_run_time: handshake succeeded, forwarding messages\n");
                forward_messages(&session, &relay_running);
            }
            else
            {
                printf("relay_run_time: failed to send CREATED reply\n");
            }
        }
        else
        {
            printf("relay_run_time: process_create_handshake failed\n");
        }
    }
    else
    {
        printf("relay_run_time: recieved UNEXPECTED message type from client (expected CREATE)\n");
    }
    
    close(user->fd);
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
    
    if(!tor_ed25519_generate_identity_keypair(g_relay_identity_pub, g_relay_identity_priv_seed))
    {
        printf("relay_run_time: failed to generate ed25519 identity keypair\n");
        retval = false;
    }
    
    if(retval)
    {
        signup_response = relay_connect_only(dir_cfg_path, &relay_listen_fd, g_relay_identity_pub);
        
        if(signup_response == NULL)
        {
            retval = false;
            printf("relay_run_time: failed to sign up relay with directory server\n");
        }
        else
        {
            relay_signup_response = signup_response;
            if(pthread_create(&accept_thread, NULL, relay_accept_loop_func, NULL) != SUCCESS)
            {
                printf("relay_run_time: failed to create accept loop thread\n");
                retval = false;
            }
            else
            {
                printf("relay_run_time: relay is running and accepting connections\n");
                relay_run_commands(); 
                pthread_join(accept_thread,NULL);
                relay_connect_signout(dir_cfg_path, &signup_response->responese_details_u.signup_response);
            }
            free(signup_response);
        }
    }
    return retval;
}