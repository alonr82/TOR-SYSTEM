#include "sock_utilities.h"

static bool tor_msg_valid(const tor_msg_t *message)
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
        
        // עכשיו אנחנו מקבלים את כל 5 סוגי ההודעות החוקיות בפרוטוקול
        if (message->header.type != TOR_MSG_EXTEND && 
            message->header.type != TOR_MSG_DATA &&
            message->header.type != TOR_MSG_CREATE &&
            message->header.type != TOR_MSG_CREATED &&
            message->header.type != TOR_MSG_EXTENDED)
        {
            retval = false;
        }
        else
        {
            // בדיקת גדלים לכל סוג הודעה שדורש גודל מוגדר מראש
            if (message->header.type == TOR_MSG_EXTEND && len != sizeof(tor_extend_t))
            {
                retval = false;
            }
            else if (message->header.type == TOR_MSG_CREATE && len != sizeof(tor_create_t))
            {
                retval = false;
            }
            else if (message->header.type == TOR_MSG_CREATED && len != sizeof(tor_created_t))
            {
                retval = false;
            }
            else if (message->header.type == TOR_MSG_EXTENDED && len != sizeof(tor_extended_t))
            {
                retval = false;
            }
        }
    }
    return retval;
}

bool read_exact(int fd, void *buf, size_t expected_size)
{
    size_t offset = 0;

    while (offset < expected_size)
    {
        ssize_t recived_bytes = recv(fd, (uint8_t*)buf + offset, expected_size - offset, 0);

        if (recived_bytes == 0)
        {
            return false; 
        }
        if (recived_bytes < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                return false;
            }
            return false;
        }
        offset += (size_t)recived_bytes;
    }

    return true;
}

bool write_exact(int fd, const void *buf, size_t expected_size)
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

bool tor_recv_msg(int fd, tor_msg_t *msg)
{
    bool retval = true;

    memset(msg, 0, sizeof(tor_msg_t));

    if(!read_exact(fd, &msg->header, sizeof(tor_header_t)))
    {
        retval = false;
    }
    else
    {
        uint16_t payload_len = ntohs(msg->header.payload_len);

        if(payload_len > (uint16_t)(sizeof(msg->payload)))
        {
            printf("tor_recv_msg: invalid payload_len (%u)\n", payload_len);
            retval = false;
        }
        else
        {
            if(payload_len > 0)
            {
                if(!read_exact(fd, msg->payload, payload_len))
                {
                    retval = false;
                }
            }
        }
    }
    return retval;
}

bool tor_send_msg(int fd, const tor_msg_t *msg)
{
    bool retval = true;

    uint16_t payload_len = ntohs(msg->header.payload_len);
    uint32_t total_len = (uint32_t)(sizeof(tor_header_t) + payload_len);

    if(total_len > sizeof(tor_msg_t))
    {
        printf("tor_send_msg: invalid payload_len (%u)\n", payload_len);
        retval = false;
    }
    else
    {
        if(!write_exact(fd, msg, total_len))
        {
            retval = false;
        }
    }
    return retval;
}

