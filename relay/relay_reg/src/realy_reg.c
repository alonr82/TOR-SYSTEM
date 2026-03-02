#include "relay_reg.h"


relay_req_res_t* relay_connect_only(const char* dir_cfg_path, int *relay_sock_fd, const uint8_t identity_pub[TOR_ID_PUB_LEN])
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
                request_t req;
                memset(&req, 0, sizeof(request_t));
                req.request_type = RELAY_REQUEST;
                req.request_u.relay_req.request_type = RELAY_REG_SIGNUP;
                *relay_sock_fd = relay_listen_on_any_port(&req.request_u.relay_req.request_details_u.signup_request.relay_port);
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
                    memcpy(req.request_u.relay_req.request_details_u.signup_request.identify_pub,
                            identity_pub, TOR_ID_PUB_LEN);
                    if (write_exact(sock_fd, &req, sizeof(request_t)) == false)
                    {
                        printf("relay: send FAIL\n");
                        retval = NULL;
                        close(*relay_sock_fd);
                    }
                    else
                    {
                        if (read_exact(sock_fd, retval, sizeof(relay_req_res_t)) == false)
                        {
                            printf("relay: recv FAIL\n");
                            retval = NULL;
                            close(*relay_sock_fd);
                        }
                        else
                        {
                            if(retval->responese_details_u.signup_response.status)
                            {
                                printf("relay: SIGNUP SUCCESS, id: %u\n", retval->responese_details_u.signup_response.relay_id);
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
            request_t req;
            memset(&req, 0, sizeof(request_t));
            req.request_type = RELAY_REQUEST;
            req.request_u.relay_req.request_type = RELAY_REG_SIGNOUT;
            req.request_u.relay_req.request_details_u.signout_request.relay_id = signup_response->relay_id;
            if (write_exact(sock_fd, &req, sizeof(request_t)) == false)
            {
                printf("relay: SEND FAILD\n");
                retval = false;
                close(sock_fd);
            }
            else
            {
                relay_req_res_t response;
                if(read_exact(sock_fd, &response, sizeof(relay_req_res_t)) == false)
                {
                    printf("relay: RECV FAILD\n");
                    retval = false;
                    close(sock_fd);
                }
                else
                {
                    if(response.responese_details_u.signout_response.status)
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