#include "sock_utilities.h"


static bool read_exact(int fd, void *buf, size_t expected_size)
{
    bool retval = true;
    size_t offset = 0;
    while (offset < expected_size) 
    {
        ssize_t recived_bytes = recv(fd, (uint8_t*)buf + offset, expected_size - offset, 0);
        if (recived_bytes == 0)
        {
            retval = false;
        }
        else if (recived_bytes < 0) 
        {
            if (errno == EINTR)
            {
                continue;
            }
            else
            {
                retval = false;
            }
        }
        offset += (size_t)recived_bytes;
    }
    return retval;
}


static bool write_exact(int fd, const void *buf, size_t expected_size)
{
    bool retval = true;
    size_t offset = 0;
    while (offset < expected_size)
    {
        ssize_t written_bytes = send(fd, (const uint8_t*)buf + offset, expected_size - offset, 0);
        if (written_bytes <= 0) 
        {
            if (written_bytes < 0 && errno == EINTR)
            {
                continue;
            }
            else
            {
                retval = false;
            }
        }
        offset += (size_t)written_bytes;
    }
    return retval;
}

bool tor_recv_msg(int fd, tor_msg_t *input)
{
    bool retval = true;
    if (!input)
    {
        retval = false;
    }
    else
    {
        if (read_exact(fd, input, sizeof(*input)) == false)
        {
            retval = false;
        }
        else if (!tor_msg_valid(input))
        {
            retval = false;
        }
    }
    return retval;
}

bool tor_send_msg(int fd, const tor_msg_t *msg)
{
    bool retval = true;
    if (!msg)
    {
        retval = false;
    }
    retval = write_exact(fd, msg, sizeof(*msg));
    return retval;
}

static  bool tor_msg_valid(const tor_msg_t *message)
{
    bool retval = true; 
    if(!message)
    {
        retval = false;
    }
    else
    {
        uint16_t len = ntohs(message->header.payload_len);
        if (len > sizeof(message->payload))
        {
            retval = false;
        } 
        if (message->header.type != TOR_MSG_EXTEND && message->header.type != TOR_MSG_DATA)
        {
            retval = false;
        }
        else
        {
            if (message->header.type == TOR_MSG_EXTEND && len != sizeof(tor_extend_t))
            {
                retval = false;
            }
        }
    }
    return retval;
}
