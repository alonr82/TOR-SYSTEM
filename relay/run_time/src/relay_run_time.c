#define _DEFAULT_SOURCE

#include "relay_run_time.h"
#include <unistd.h> 
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>

#define MALICIOUS_REGISTRY_DIR "simulation"
#define MALICIOUS_REGISTRY_PATH "simulation/malicious_relays_registry.txt"

static int relay_listen_fd = -1;
static relay_req_res_t* relay_signup_response;
static volatile bool relay_running = true;
static pthread_t accept_thread;

uint8_t g_relay_identity_pub[TOR_ED25519_PUB_LEN];
uint8_t g_relay_identity_priv_seed[TOR_ED25519_SEED_LEN];
relay_runtime_mode_e g_relay_runtime_mode = relay_runtime_mode_normal;

static relay_runtime_mode_e get_relay_mode_from_environment(void)
{
    relay_runtime_mode_e retval = relay_runtime_mode_normal;
    const char* mode_env = getenv("TOR_RELAY_MODE");

    if(mode_env != NULL)
    {
        if(strcmp(mode_env, "malicious") == 0)
        {
            retval = relay_runtime_mode_attacker_controlled;
        }
    }

    return retval;
}

static bool ensure_simulation_directory_exists(void)
{
    bool retval = true;
    struct stat st;

    if(stat(MALICIOUS_REGISTRY_DIR, &st) == 0)
    {
        if(S_ISDIR(st.st_mode) == 0)
        {
            retval = false;
        }
    }
    else
    {
        if(mkdir(MALICIOUS_REGISTRY_DIR, 0700) != 0)
        {
            if(errno != EEXIST)
            {
                retval = false;
            }
        }
    }

    return retval;
}

static void bytes_to_hex_string(const uint8_t* input, uint32_t input_len, char* output, uint32_t output_size)
{
    static const char* hex_chars = "0123456789ABCDEF";
    uint32_t index = 0;

    if(input != NULL && output != NULL && output_size >= (input_len * 2 + 1))
    {
        for(index = 0; index < input_len; index++)
        {
            output[index * 2] = hex_chars[(input[index] >> 4) & 0x0F];
            output[index * 2 + 1] = hex_chars[input[index] & 0x0F];
        }
        output[input_len * 2] = '\0';
    }
}

static bool register_attacker_identity_in_simulation_registry(const uint8_t identity_pub[TOR_ED25519_PUB_LEN])
{
    bool retval = true;
    FILE* registry_file = NULL;
    char identity_hex[TOR_ED25519_PUB_LEN * 2 + 1];

    if(ensure_simulation_directory_exists() == false)
    {
        retval = false;
    }
    else
    {
        memset(identity_hex, 0, sizeof(identity_hex));
        bytes_to_hex_string(identity_pub, TOR_ED25519_PUB_LEN, identity_hex, sizeof(identity_hex));

        registry_file = fopen(MALICIOUS_REGISTRY_PATH, "a");
        if(registry_file == NULL)
        {
            retval = false;
        }
        else
        {
            fprintf(registry_file, "%s\n", identity_hex);
            fclose(registry_file);
        }
    }

    return retval;
}

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

    g_relay_runtime_mode = get_relay_mode_from_environment();
    
    if(g_relay_runtime_mode == relay_runtime_mode_attacker_controlled)
    {
        printf("relay_run_time: starting relay in attacker-controlled simulation mode\n");
    }
    else
    {
        printf("relay_run_time: starting relay in normal mode\n");
    }
    
    if(!tor_ed25519_generate_identity_keypair(g_relay_identity_pub, g_relay_identity_priv_seed))
    {
        printf("relay_run_time: failed to generate ed25519 identity keypair\n");
        retval = false;
    }

    if(retval == true)
    {
        if(g_relay_runtime_mode == relay_runtime_mode_attacker_controlled)
        {
            if(register_attacker_identity_in_simulation_registry(g_relay_identity_pub) == false)
            {
                printf("relay_run_time: failed to register attacker-controlled identity in simulation registry\n");
                retval = false;
            }
        }
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