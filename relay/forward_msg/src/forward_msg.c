#include "forward_msg.h"

bool extend_connection(session_t *session, tor_msg_t *msg)
{
    bool retval = true;
    int extended_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(extended_fd < 0)
    {
        printf("Failed to create socket for extend connection\n");
        retval = false;
    }
    else
    {
        tor_extend_t extend_msg_recived;
        if(msg->header.type == TOR_MSG_EXTEND)
        {
            memcpy(&extend_msg_recived,msg->payload,sizeof(extend_msg_recived));
            struct sockaddr_in addr;
            memset(&addr,0,sizeof(addr));
            addr.sin_family = AF_INET;
            addr.sin_port = extend_msg_recived.port;
            addr.sin_addr.s_addr = extend_msg_recived.ip_v4;
            int connect_result = connect(extended_fd,(struct sockaddr*)&addr, sizeof(addr));
            if(connect_result < 0)
            {
                printf("Failed to connect to extended socket\n");
                close(extended_fd);
                retval = false;
            }
            else
            {
                session->next_fd = extended_fd;
            }
        }
        else
        {
            retval = false;
        }
    }
    return retval;
}

void forward_messages(int last_fd, int next_fd)
{
    uint8_t buf[TOR_MSG_SIZE];
    struct pollfd fds[TWO_SOCKETS];
    fds[0].events = POLLIN;
    fds[0].fd = last_fd;
    fds[1].events = POLLIN;
    fds[1].fd = next_fd;
    while(true)
    {
        int poll_resualt = poll(fds, TWO_SOCKETS, -1);
        if(poll_resualt < 0)
        {
            if(errno == EINTR)
            {
                continue;
            }
            else
            {
                break;
            }
        }
        else
        {
            if(fds[0].revents & POLLIN)
            {
                ssize_t bytes_recived = recv(last_fd,buf,TOR_MSG_SIZE,0);
                if(bytes_recived > 0)
                {
                    if(!(write_exact(next_fd,buf,bytes_recived)))
                    {
                        printf("failed to write exact\n");
                        break;
                    }
                }
                else
                {
                    printf("bytes recived <= 0\n");
                    break;
                }
            }
            if(fds[1].revents & POLLIN)
            {
                ssize_t bytes_recived = recv(next_fd,buf,TOR_MSG_SIZE,0);
                if(bytes_recived > 0)
                {
                    if(!(write_exact(last_fd,buf,bytes_recived)))
                    {
                        printf("failed to write exact\n");
                        break;
                    }
                }
                else
                {
                    printf("bytes recived <= 0\n");
                    break;
                }
            }
        }
    }
}