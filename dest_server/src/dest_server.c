#include "dest_server.h"
#include <stdbool.h>
#include <pthread.h>

static volatile bool server_running = true;
static int g_dest_fd = -1;

void dest_run_commands(void)
{
    char input[128];
    while (server_running)
    {
        if (fgets(input, sizeof(input), stdin) == NULL)
        {
            break;
        }
        input[strcspn(input, "\n")] = '\0';
        if (strcmp(input, "exit") == 0)
        {
            printf("[Dest Server] Shutting down...\n");
            server_running = false;

            if (g_dest_fd >= 0)
            {
                // שובר את ה-accept שנמצא במצב חוסם
                shutdown(g_dest_fd, SHUT_RDWR);
                close(g_dest_fd);
                g_dest_fd = -1;
            }
            break;
        }
    }
}

int dest_listen_socket()
{
    int dest_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(dest_fd < 0)
    {
        perror("socket");
    }
    else
    {
        struct sockaddr_in dest_addr;
        memset(&dest_addr, 0, sizeof(dest_addr));
        int opt = 1;
        setsockopt(dest_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        dest_addr.sin_family = AF_INET;
        dest_addr.sin_addr.s_addr = INADDR_ANY;
        dest_addr.sin_port = htons(DEST_SERVER_PORT);

        if(bind(dest_fd, (struct sockaddr*)&dest_addr, sizeof(dest_addr)) < 0)
        {
            perror("bind");
            close(dest_fd);
            dest_fd = -1;
        }
        else if(listen(dest_fd, BACKLOG_NUM) < 0)
        {
            perror("listen");
            close(dest_fd);
            dest_fd = -1;
        }
    }
    g_dest_fd = dest_fd; 
    return dest_fd;
}

void dest_accept_loop(int dest_fd)
{
    while(server_running)
    {
        struct sockaddr_in client_addr;
        memset(&client_addr, 0, sizeof(client_addr));
        socklen_t client_addr_len = sizeof(client_addr);
        
        int client_fd = accept(dest_fd, (struct sockaddr*)&client_addr, &client_addr_len);
        
        if (client_fd < 0)
        {
            if (!server_running)
            {
                break;
            } 
            perror("accept");
            continue;
        }

        pthread_t thread_id;
        if(pthread_create(&thread_id, NULL, dest_thread, (void*)(intptr_t)client_fd) != 0)
        {
            perror("pthread_create");
            close(client_fd);
            continue;
        }
        pthread_detach(thread_id);
    }
    printf("[Dest Server] Accept loop finished.\n");
}



bool run_server()
{
    int fd = dest_listen_socket();
    if (fd < 0)
    {
        return false;
    } 

    printf("[Dest Server] Listening on port %d. Type 'exit' to stop.\n", DEST_SERVER_PORT);

    // מריצים את ה-accept בת'רד נפרד כדי שה-main יוכל לקלוט פקודות
    pthread_t accept_tid;
    if (pthread_create(&accept_tid, NULL, (void* (*)(void*))dest_accept_loop, (void*)(intptr_t)fd) != 0)
    {
        close(fd);
        return false;
    }

    dest_run_commands(); // כאן התוכנית מחכה ל-'exit'

    pthread_join(accept_tid, NULL);
    return true;
}

bool dest_handle_message(int client_fd)
{
    tor_msg_t message;
    memset(&message, 0, sizeof(message));
    
    if(tor_recv_msg(client_fd, &message))
    {
        if(message.header.type == TOR_MSG_DATA)
        {
            uint8_t key[E2E_KEY_LEN]; 
            memset(key, 0x42, E2E_KEY_LEN);
            uint16_t blob_len_host = ntohs(message.header.payload_len);
            // 1. הגדרת באפר מספיק גדול להודעה המפוענחת
            uint8_t plaintext[sizeof(message.payload)];
            uint16_t plaintext_len = 0;
            // 2. שליחת המשתנים הנכונים (בלי & על המערך עצמו)
            if(decrypt_blob(message.payload,&blob_len_host, key, plaintext, &plaintext_len))
            {
                // 3. הוספת Null-terminator כדי להדפיס כסטרינג בבטחה
                if (plaintext_len < sizeof(plaintext)) {
                    plaintext[plaintext_len] = '\0';
                }
                printf("Received message: %s\n", (char*)plaintext);
            }
        }
    }
    return false;
}

void * dest_thread(void * arg)
{
    int client_fd = (int)(intptr_t)arg;
    dest_handle_message(client_fd); 
    close(client_fd);
    return NULL;
}
