#define _POSIX_C_SOURCE 200112L
#include "sock_utilities.h"

static void (*g_connection_handler)(user_descriptor_t* user) = NULL;

static pthread_mutex_t g_active_threads_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  g_active_threads_cv    = PTHREAD_COND_INITIALIZER;
static int g_active_threads_count = 0;

static void active_threads_inc(void)
{
    pthread_mutex_lock(&g_active_threads_mutex);
    g_active_threads_count++;
    pthread_mutex_unlock(&g_active_threads_mutex);
}

static void active_threads_dec(void)
{
    pthread_mutex_lock(&g_active_threads_mutex);
    g_active_threads_count--;
    if (g_active_threads_count == 0)
    {
        pthread_cond_signal(&g_active_threads_cv);
    }
    pthread_mutex_unlock(&g_active_threads_mutex);
}

static void wait_for_all_threads_to_finish(void)
{
    pthread_mutex_lock(&g_active_threads_mutex);
    while (g_active_threads_count > 0)
    {
        pthread_cond_wait(&g_active_threads_cv, &g_active_threads_mutex);
    }
    pthread_mutex_unlock(&g_active_threads_mutex);
}

static void* connection_handler_wrapper(void* arg)
{
    user_descriptor_t* user_desc = (user_descriptor_t*)arg;
    if (g_connection_handler != NULL)
    {
        g_connection_handler(user_desc);
    }

    /* חשוב: ה-callback לא עושה free(user_desc). */
    free(user_desc);

    active_threads_dec();
    return NULL;
}

void accept_loop(int server_fd, void (*connection_handler)(user_descriptor_t* user))
{
    g_connection_handler = connection_handler;

    while (true)
    {
        struct sockaddr_storage client_addr;
        socklen_t client_addr_len = sizeof(client_addr);

        int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_addr_len);
        if (client_fd < 0)
        {
            if ((errno == EBADF) || (errno == EINVAL))
            {
                break;
            }
            if (errno == EINTR)
            {
                continue;
            }
            if ((errno == EAGAIN) || (errno == EWOULDBLOCK))
            {
                continue;
            }
            break;
        }

        user_descriptor_t* user_desc = malloc(sizeof(user_descriptor_t));
        if (user_desc == NULL)
        {
            close(client_fd);
            continue;
        }

        user_desc->fd = client_fd;
        user_desc->addr = client_addr;
        user_desc->addr_len = client_addr_len;

        pthread_t thread_id;
        active_threads_inc();
        if (pthread_create(&thread_id, NULL, connection_handler_wrapper, (void*)user_desc) != 0)
        {
            active_threads_dec();
            close(client_fd);
            free(user_desc);
            continue;
        }

        /* נשאר detached - אבל אנחנו מחכים שכולם יסתיימו לפני יציאה מה-loop */
        pthread_detach(thread_id);
    }

    /* כדי ש-valgrind לא יראה TLS/stack "possibly lost" */
    wait_for_all_threads_to_finish();
}

void set_socket_nonblock(int sock_fd)
{
    int flags = fcntl(sock_fd, F_GETFL, 0);
    if (flags < 0)
    {
        return;
    }
    fcntl(sock_fd, F_SETFL, flags | O_NONBLOCK);
}
