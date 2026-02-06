// חובה להוסיף את השורה הזו ראשונה כדי למנוע את האזהרה על usleep
#define _DEFAULT_SOURCE 

#include "relay_run_time.h"
#include <unistd.h> // בשביל usleep

static int relay_listen_fd = -1;
static relay_req_res_t* relay_signup_response;
static volatile bool relay_running = true;
static pthread_t accept_thread;

static void relay_run_commands(void)
{
    char input[128];

    while (relay_running)
    {
        // שימוש ב-select כדי לא לחסום את ה-fgets לנצח
        struct timeval tv = {0, 500000}; // 0.5 שניות
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
    
    // אם הריליי בתהליך סגירה, לא נקבל הודעות חדשות
    if(!relay_running) {
        close(user->fd);
        return;
    }

    if(tor_recv_msg(user->fd, &message) != true)
    {
        close(user->fd);
        return;
    }
    
    if(message.header.type == TOR_MSG_EXTEND)
    {
        session_t session;
        session.last_fd = user->fd;
        session.next_fd = -1;
        if((extend_connection(&session,&message)))
        {
            extend_ack_e ack = ACK_EXTEND_OK;
            if(write_exact(session.last_fd, &ack, sizeof(extend_ack_e)))
            {
                printf("relay_run_time: extend_connection succeeded, forwarding messages\n");
                // מעבירים את הפוינטר לדגל הגלובלי
                forward_messages(session.last_fd, session.next_fd, &relay_running);
                
                close(session.next_fd);
                close(session.last_fd);
                return;
            }
        }
        else
        {
            extend_ack_e ack = ACK_EXTEND_FAIL;
            write_exact(session.last_fd, &ack, sizeof(extend_ack_e));
            printf("relay_run_time: extend_connection failed\n");
        }
    }
    else if(message.header.type == TOR_MSG_DATA)
    {
        int dest_fd = connect_to_dest_server();
        if(dest_fd < 0)
        {
            printf("relay_run_time: failed to connect to destination server\n");
            close(user->fd);
            return;
        }
        else
        {
            tor_send_msg(dest_fd, &message);
            forward_messages(user->fd, dest_fd, &relay_running);
            close(dest_fd);
            close(user->fd);
            return;
        }
    }
    else
    {
        printf("relay_run_time: recieved UNKNOWN message type from client\n");
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
    signup_response = relay_connect_only(dir_cfg_path, &relay_listen_fd);
    
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
    return retval;
}