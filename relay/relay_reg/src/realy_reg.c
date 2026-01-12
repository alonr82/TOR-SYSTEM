#include "relay_reg.h"


relay_req_res_t* relay_connect_only(const char* dir_cfg_path, int *relay_sock_fd)
{
    relay_req_res_t* retval = calloc(1, sizeof(relay_req_res_t));
    if(retval != NULL)
    {
        server_config_metadata_t* cfg = fetch_server_config((char*)dir_cfg_path);
        if (!cfg)
        {
            printf("relay: bad config\n");
            retval = NULL;
        }
        else
        {
            int sock_fd = connect_server(cfg);
            if (sock_fd < 0)
            {
                printf("relay: connect FAIL\n");
                retval = NULL;
            }
            else
            {
                relay_req_t req;
                memset(&req, 0, sizeof(relay_req_t));
                req.request_type = RELAY_REG_SIGNUP;
                *relay_sock_fd = relay_listen_on_any_port(&req.signup_request.relay_port);
                if (*relay_sock_fd < 0) 
                {
                    printf("relay: listen FAIL\n");
                    retval = NULL;
                    close(sock_fd);
                    free(cfg);
                    retval = NULL;
                }
                else
                {
                    ssize_t sent_bytes = send(sock_fd, &req, sizeof(relay_req_t), 0);
                    if (sent_bytes != sizeof(relay_req_t))
                    {
                        printf("relay: send FAIL\n");
                        retval = NULL;
                        close(relay_sock_fd);
                    }
                    else
                    {
                        ssize_t recv_bytes = recv(sock_fd, retval, sizeof(relay_req_res_t),0);
                        if(recv_bytes != sizeof(relay_req_res_t))
                        {
                            printf("relay: recv FAIL\n");
                            retval = NULL;
                            close(relay_sock_fd);
                        }
                        else
                        {
                            if(retval->signup_response.status)
                            {
                                printf("relay: SIGNUP SUCCESS, id: %u\n", retval->signup_response.relay_id);
                            }
                            else
                            {
                                printf("relay: signup FAIL\n");
                            }
                        }
                    }
                }
            }
            free(cfg);
            close(sock_fd);
        }
    }
    return retval;
}

bool relay_connect_signout(const char *dif_cfg,relay_signup_response_t * signup_response)
{
    bool retval = true;
    server_config_metadata_t* cfg = fetch_server_config((char*)dif_cfg);
    if (!cfg)
    {
        printf("relay: BAD CONFIG\n");
        retval = false;
    }
    else
    {
        int sock_fd = connect_server(cfg);
        if(sock_fd < 0)
        {
            printf("relay: CONNECT FAIL\n");
            retval = false;
        }
        else
        {
            relay_req_t req;
            memset(&req, 0, sizeof(relay_req_t));
            req.request_type = RELAY_REG_SIGNOUT;
            req.signout_request.relay_id = signup_response->relay_id;
            ssize_t sent_bytes = send(sock_fd, &req, sizeof(relay_req_t), 0);
            if (sent_bytes != sizeof(relay_req_t))
            {
                printf("relay: SEND FAILD\n");
                retval = false;
                close(sock_fd);
            }
            else
            {
                relay_req_res_t response;
                ssize_t recv_bytes = recv(sock_fd, &response, sizeof(relay_req_res_t),0);
                if(recv_bytes != sizeof(relay_req_res_t))
                {
                    printf("relay: RECV FAILD\n");
                    retval = false;
                    close(sock_fd);
                }
                else
                {
                    if(response.signout_response.status)
                    {
                        printf("relay: SIGNOUT SUCCESS, id: %u\n", signup_response->relay_id);
                    }
                    else
                    {
                        printf("relay: SIGNOUT FAILD\n");
                        retval = false;
                    }
                    close(sock_fd);
                }
            }
        }
    }
    free(cfg);
    return retval;
}