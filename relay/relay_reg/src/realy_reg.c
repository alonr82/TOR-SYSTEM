#include "relay_reg.h"


relay_code_e relay_connect_only(const char* dir_cfg_path)
{
    relay_code_e retval = relay_success;
    server_config_metadata_t* cfg = fetch_server_config((char*)dir_cfg_path);
    if (!cfg)
    {
        printf("relay: bad config\n");
        retval = relay_error;
    }
    else
    {
        int sock_fd = connect_server(cfg);
        if (sock_fd < 0)
        {
            printf("relay: connect FAIL\n");
            retval = relay_error;
        }
        else
        {
            relay_request_t req;
            memset(&req, 0, sizeof(relay_request_t));
            req.request_type = RELAY_REG_SIGNUP;
            req.relay_port = 9000;
            ssize_t sent_bytes = send(sock_fd, &req, sizeof(relay_request_t), 0);
            if (sent_bytes != sizeof(relay_request_t))
            {
                printf("relay: send FAIL\n");
                retval = relay_error;
            }
            else
            {
                relay_req_response_t response;
                ssize_t recv_bytes = recv(sock_fd, &response, sizeof(relay_req_response_t),0);
                if(recv_bytes != sizeof(relay_req_response_t))
                {
                    printf("relay: recv FAIL\n");
                    retval = relay_error;
                }
                else
                {
                    if(response.status)
                    {
                        printf("relay: SIGNUP SUCCESS, id: %u\n", response.relay_id);
                    }
                    else
                    {
                        printf("relay: signup FAIL\n");
                        retval = relay_error;
                    }
                }
            }
        }
    }
    free(cfg);
    return retval;
}