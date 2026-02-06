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

void forward_messages(int last_fd, int next_fd, volatile bool *run_flag)
{
    uint8_t buf[TOR_MSG_SIZE];
    struct pollfd fds[TWO_SOCKETS];
    fds[0].events = POLLIN;
    fds[0].fd = last_fd;
    fds[1].events = POLLIN;
    fds[1].fd = next_fd;

    while(*run_flag)
    {
        int poll_resualt = poll(fds, TWO_SOCKETS, 50); 
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
        if(poll_resualt == 0)
        {
            continue;
        } 

        if(fds[0].revents & POLLIN)
        {
            ssize_t bytes_recived = recv(last_fd, buf, TOR_MSG_SIZE, 0);
            if(bytes_recived > 0)
            {
                if(!(write_exact(next_fd, buf, bytes_recived)))
                {
                    break;
                } 
                tor_msg_t* msg = (tor_msg_t*)buf;
                int payload_len = ntohs(msg->header.payload_len);
                printf("message recieved: %.*s\n", payload_len, (char*)msg->payload);
            }
            else 
            {
                break;
            }

        }
        
        if(fds[1].revents & POLLIN)
        {
            ssize_t bytes_recived = recv(next_fd, buf, TOR_MSG_SIZE, 0);
            if(bytes_recived > 0)
            {
                if(!(write_exact(last_fd, buf, bytes_recived)))
                {
                    break;
                } 
                tor_msg_t* msg = (tor_msg_t*)buf;
                int payload_len = ntohs(msg->header.payload_len);
                printf("message recieved: %.*s\n", payload_len, (char*)msg->payload);
            }
            else 
            {
                break;
            }
        }
    }
    shutdown(last_fd, SHUT_RDWR);
    if(next_fd >= 0)
    {
        shutdown(next_fd, SHUT_RDWR);
    } 
}

int connect_to_dest_server(void)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
    {
        perror("socket");
    }
    else
    {
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(DEST_SERVER_PORT);
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK); 
        if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0)
        {
            perror("connect dest");
            close(fd);
        }
    }
    return fd;
}
