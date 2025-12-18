#include "handlers.h"

handler_status_e process_relay(int sock_fd, msg_server_buffer_t* buffer)
{
    handler_status_e retval = handler_succeded;
    if (buffer == NULL)
    {
        retval = handler_failed;
    }
    else
    {
        relay_data_t* relay = NULL;
        bool running = true;
        while (running)
        {
            // if (handle_message(sock_fd, &relay, buffer, &running) == false)
            // {
            //     free_socket(sock_fd, NULL);
            //     close(sock_fd);
            //     retval = handler_failed;
            // }
            sleep(10);
        }
    }
    // free_socket(sock_fd, NULL);
    close(sock_fd);
    return retval;
}