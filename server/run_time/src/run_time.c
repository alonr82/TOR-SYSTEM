#include "run_time.h"
#include "tor_crypto.h"
#include "simulation_state.h"
#include "simulation_events.h"
#include <time.h>
#include <stdlib.h>
#include <string.h>

#define MALICIOUS_REGISTRY_PATH "simulation/malicious_relays_registry.txt"
#define GUARD_THRESHOLD_SECONDS 300

static int server_socket_fd;

static const uint8_t DIR_SERVER_PRIV_KEY[32] = {
    0x9d, 0x61, 0xb1, 0x9d, 0xef, 0xfd, 0x5a, 0x60, 
    0xba, 0x84, 0x4a, 0xf4, 0x92, 0xec, 0x2c, 0xc4, 
    0x44, 0x49, 0xc5, 0x69, 0x7b, 0x32, 0x69, 0x19, 
    0x70, 0x3b, 0xac, 0x03, 0x1c, 0xae, 0x7f, 0x60
};

static void print_simulation_state(void)
{
    simulation_state_t state = simulation_state_get_copy();

    printf("[SIM STATE] sybil_attack=%u sybil_defense=%u replay_attack=%u replay_defense=%u circ_ext_attack=%u circ_ext_defense=%u onion_bomb_attack=%u onion_bomb_defense=%u fingerprint_attack=%u fingerprint_defense=%u mock_cpu=%u\n",
           state.attack_sybil_active,
           state.def_sybil_active,
           state.attack_replay_active,
           state.def_replay_active,
           state.attack_circ_ext_active,
           state.def_circ_ext_active,
           state.attack_onion_bomb_active,
           state.def_onion_bomb_active,
           state.attack_fingerprint_active,
           state.def_fingerprint_active,
           state.mock_cpu_load);
    fflush(stdout);
}

static void bytes_to_hex_string(const uint8_t* input, uint32_t input_len, char* output, uint32_t output_size)
{
    static const char* hex_chars = "0123456789ABCDEF";
    uint32_t index = 0;

    if(input != NULL && output != NULL)
    {
        if(output_size >= (input_len * 2 + 1))
        {
            for(index = 0; index < input_len; index++)
            {
                output[index * 2] = hex_chars[(input[index] >> 4) & 0x0F];
                output[index * 2 + 1] = hex_chars[input[index] & 0x0F];
            }

            output[input_len * 2] = '\0';
        }
    }
}

static bool identity_is_attacker_controlled_in_registry(const uint8_t identify_pub[TOR_ID_PUB_LEN])
{
    bool retval = false;
    FILE* registry_file = NULL;
    char current_line[256];
    char identity_hex[TOR_ID_PUB_LEN * 2 + 1];

    memset(identity_hex, 0, sizeof(identity_hex));
    bytes_to_hex_string(identify_pub, TOR_ID_PUB_LEN, identity_hex, sizeof(identity_hex));

    registry_file = fopen(MALICIOUS_REGISTRY_PATH, "r");

    if(registry_file != NULL)
    {
        while(fgets(current_line, sizeof(current_line), registry_file) != NULL)
        {
            current_line[strcspn(current_line, "\r\n")] = '\0';

            if(strcmp(current_line, identity_hex) == 0)
            {
                retval = true;
                break;
            }
        }

        fclose(registry_file);
    }

    return retval;
}

static uint32_t get_uptime_seconds(const relay_decript_t *relay)
{
    uint32_t retval = 0;
    time_t current_time = 0;

    if(relay != NULL)
    {
        current_time = time(NULL);

        if(current_time >= relay->registration_time)
        {
            retval = (uint32_t)(current_time - relay->registration_time);
        }
    }

    return retval;
}

static bool is_hardcoded_core_guard(const uint8_t relay_ip[IP6_SIZE])
{
    if (relay_ip[0] == 172 && relay_ip[1] == 28 && relay_ip[2] == 0)
    {
        if (relay_ip[3] == 21 || relay_ip[3] == 22)
        {
            return true;
        }
    }
    return false;
}

static void refresh_relay_flags(relay_decript_t *relay)
{
    if(relay != NULL)
    {
        relay->uptime_seconds = get_uptime_seconds(relay);
        relay->is_exit = relay->is_active;

        if(is_hardcoded_core_guard(relay->relay_ip) == true)
        {
            relay->is_guard = true; 
        }
        else if(relay->uptime_seconds >= GUARD_THRESHOLD_SECONDS)
        {
            relay->is_guard = true; 
        }
        else
        {
            relay->is_guard = false;
        }

        relay->eligible_guard = relay->is_guard;
    }
}

static void apply_relay_flags_to_export(relay_decript_t* relay_list, uint32_t fetched_relays, bool defense_enabled)
{
    for(uint32_t index = 0; index < fetched_relays; index++)
    {
        refresh_relay_flags(&relay_list[index]);

        if(defense_enabled == false)
        {
            if(relay_list[index].is_active == true)
            {
                relay_list[index].is_guard = true;
                relay_list[index].eligible_guard = true;
            }
        }
    }
}

static void run_commands()
{
    bool running = true;

    while (running == true)
    {
        char input[INPUT_SIZE];

        if (fgets(input, sizeof(input), stdin) != input)
        {
            fprintf(stderr, "Invalid input");
            break;
        }

        input[strcspn(input, "\n")] = '\0';
        printf("`%s`\n", input);

        if (strcmp(input, "exit") == 0)
        {
            running = false;
        }
        else if (strcmp(input, "sim show") == 0)
        {
            print_simulation_state();
        }
        else if (strcmp(input, "sim sybil attack on") == 0)
        {
            if(simulation_state_set_flag("sybil", false, true) == true)
            {
                simulation_log_warn("Sybil attack enabled");
            }
        }
        else if (strcmp(input, "sim sybil attack off") == 0)
        {
            if(simulation_state_set_flag("sybil", false, false) == true)
            {
                simulation_log_info("Sybil attack disabled");
            }
        }
        else if (strcmp(input, "sim sybil defense on") == 0)
        {
            if(simulation_state_set_flag("sybil", true, true) == true)
            {
                simulation_log_info("Sybil defense enabled");
            }
        }
        else if (strcmp(input, "sim sybil defense off") == 0)
        {
            if(simulation_state_set_flag("sybil", true, false) == true)
            {
                simulation_log_info("Sybil defense disabled");
            }
        }
        else if (strcmp(input, "sim reset") == 0)
        {
            simulation_state_reset();
            simulation_log_info("Simulation state reset");
        }
        else if (strcmp(input, "sim circ_ext attack on") == 0)
        {
            if(simulation_state_set_flag("circ_ext", false, true) == true)
            {
                simulation_log_warn("Circuit Extension attack enabled");
            }
        }
        else if (strcmp(input, "sim circ_ext attack off") == 0)
        {
            if(simulation_state_set_flag("circ_ext", false, false) == true)
            {
                simulation_log_info("Circuit Extension attack disabled");
            }
        }
        else if (strcmp(input, "sim circ_ext defense on") == 0)
        {
            if(simulation_state_set_flag("circ_ext", true, true) == true)
            {
                simulation_log_info("Circuit Extension defense enabled");
            }
        }
        else if (strcmp(input, "sim circ_ext defense off") == 0)
        {
            if(simulation_state_set_flag("circ_ext", true, false) == true)
            {
                simulation_log_info("Circuit Extension defense disabled");
            }
        }
    }
}

static uint32_t hide_attacker_controlled_relays(relay_decript_t* relay_list, uint32_t fetched_relays)
{
    uint32_t retval = 0;
    uint32_t hidden_amount = 0;

    for(uint32_t index = 0; index < fetched_relays; index++)
    {
        if(relay_list[index].is_malicious == false)
        {
            relay_list[index].is_filtered = false;

            if(retval != index)
            {
                relay_list[retval] = relay_list[index];
            }

            retval++;
        }
        else
        {
            relay_list[index].is_filtered = true;
            hidden_amount++;
        }
    }

    if(hidden_amount > 0)
    {
        simulation_log_info("Attack is disabled, hiding %u attacker-controlled relays", hidden_amount);
    }

    return retval;
}

static void log_exported_relays(relay_decript_t* relay_list, uint32_t fetched_relays)
{
    for(uint32_t index = 0; index < fetched_relays; index++)
    {
        printf("Relay index: %u, Relay ID: %u, IP adress: %u.%u.%u.%u, Port: %u, Active: %u, Malicious(sim): %u, Guard: %u, Exit: %u, EligibleGuard: %u, Trust: %u, SameSource: %u, Uptime: %u\n",
               index,
               relay_list[index].relay_id,
               relay_list[index].relay_ip[0], relay_list[index].relay_ip[1],
               relay_list[index].relay_ip[2], relay_list[index].relay_ip[3],
               relay_list[index].relay_port,
               relay_list[index].is_active,
               relay_list[index].is_malicious,
               relay_list[index].is_guard,
               relay_list[index].is_exit,
               relay_list[index].eligible_guard,
               relay_list[index].trust_score,
               relay_list[index].same_source_count,
               relay_list[index].uptime_seconds);
    }
    // התיקון הקריטי! משחרר את הנתונים ל-Docker מיד!
    fflush(stdout); 
}

static bool handle_send_relay(int client_fd)
{
    bool retval = true;
    uint32_t start = 0;
    relay_decript_t *relay_list = NULL;
    uint32_t fetched_relays = 0;
    simulation_state_t simulation_state;

    relay_list = calloc(MAX_RELAY_BATCH_SIZE, sizeof(relay_decript_t));

    if(relay_list == NULL)
    {
        retval = false;
    }
    else
    {
        fetched_relays = get_relay_batch(relay_list, &start, MAX_RELAY_BATCH_SIZE);
        simulation_state = simulation_state_get_copy();

        // --- התיקון הארכיטקטוני שלך: הפעלת הסינון כשהסימולציה כבויה ---
        if(simulation_state.attack_circ_ext_active == false && simulation_state.attack_sybil_active == false)
        {
            fetched_relays = hide_attacker_controlled_relays(relay_list, fetched_relays);
        }
        // -------------------------------------------------------------

        if(simulation_state.def_sybil_active == false && simulation_state.attack_sybil_active == true)
        {
            simulation_log_warn("Sybil attack active without defense: attacker-controlled relays are visible to clients");
        }
        else if(simulation_state.def_sybil_active == true)
        {
            simulation_log_info("Sybil defense active: only mature relays keep Guard flag");
        }

        apply_relay_flags_to_export(relay_list, fetched_relays, simulation_state.def_sybil_active);

        log_exported_relays(relay_list, fetched_relays);

        if(write_exact(client_fd, &fetched_relays, sizeof(uint32_t)) == false)
        {
            retval = false;
        }
        else
        {
            uint8_t signature[64];

            tor_ed25519_sign(signature, DIR_SERVER_PRIV_KEY, (const uint8_t*)relay_list, fetched_relays * sizeof(relay_decript_t));

            if(write_exact(client_fd, signature, 64) == false)
            {
                retval = false;
            }
            else if(write_exact(client_fd, relay_list, fetched_relays * sizeof(relay_decript_t)) == false)
            {
                retval = false;
            }
        }

        free(relay_list);
    }

    return retval;
}

relay_decript_t* get_data_from_req(user_descriptor_t* user, relay_req_t* request)
{
    relay_decript_t* retval = NULL;
    struct sockaddr_in* addr_in = NULL;

    retval = calloc(1, sizeof(relay_decript_t));

    if(retval != NULL)
    {
        addr_in = (struct sockaddr_in*)&user->addr;
        retval->ip_type = 4;
        memcpy(retval->relay_ip, &addr_in->sin_addr, IP4_SIZE);
        retval->relay_port = request->request_details_u.signup_request.relay_port;
        retval->is_active = true;
        retval->assigned_count = 0;
        retval->is_malicious = identity_is_attacker_controlled_in_registry(request->request_details_u.signup_request.identify_pub);
        retval->is_verified_historically = false;
        retval->is_filtered = false;
        retval->eligible_guard = false;
        retval->is_guard = false;
        retval->is_exit = true;
        retval->trust_score = 0;
        retval->same_source_count = 0;
        retval->source_ip_v4 = 0;
        retval->registration_time = time(NULL);
        retval->uptime_seconds = 0;
        retval->first_seen_unix_sec = (uint64_t)retval->registration_time;

        memcpy(retval->identify_pub,
               request->request_details_u.signup_request.identify_pub,
               TOR_ID_PUB_LEN);
    }

    return retval;
}

static void client_callback(user_descriptor_t* user)
{
    relay_req_res_t retval = {0};
    request_t request = {0};

    if (read_exact(user->fd, &request, sizeof(request_t)) == false)
    {
        fprintf(stderr, "Failed to receive full request\n");
        close(user->fd);
        retval.responese_details_u.signup_response.status = false;
        retval.responese_details_u.signout_response.status = false;
    }
    else
    {
        if(request.request_type == RELAY_REQUEST && request.request_u.relay_req.request_type == RELAY_REG_SIGNUP)
        {
            relay_decript_t* relay_data = NULL;
            relay_data_t* new_relay = NULL;

            relay_data = get_data_from_req(user,&request.request_u.relay_req);

            if(relay_data != NULL)
            {
                new_relay = generate_relay(relay_data);

                printf("Relay data: IP address: %u.%u.%u.%u, Port: %u\n",
                       relay_data->relay_ip[0],
                       relay_data->relay_ip[1],
                       relay_data->relay_ip[2],
                       relay_data->relay_ip[3],
                       relay_data->relay_port);

                free(relay_data);
            }

            if (new_relay == NULL)
            {
                fprintf(stderr, "Failed to generate new relay\n");
                close(user->fd);
                retval.responese_details_u.signup_response.status = false;
            }
            else
            {
                retval.responese_details_u.signup_response.relay_id = new_relay->relay_id;
                retval.responese_details_u.signup_response.status = true;

                if (write_exact(user->fd, &retval, sizeof(relay_req_res_t)) == false)
                {
                    fprintf(stderr, "Failed to send full response\n");
                    close(user->fd);
                    retval.responese_details_u.signup_response.status = false;
                }
                else
                {
                    printf("Registered new relay with ID: %u | Trust: %u | SameSource: %u | Malicious(sim): %u | Guard: %u | Uptime: %u\n",
                           new_relay->relay_id,
                           new_relay->descriptor.trust_score,
                           new_relay->descriptor.same_source_count,
                           new_relay->descriptor.is_malicious,
                           new_relay->descriptor.is_guard,
                           new_relay->descriptor.uptime_seconds);
                    fflush(stdout);
                    close(user->fd);
                }
            }
        }
        else if(request.request_type == RELAY_REQUEST && request.request_u.relay_req.request_type == RELAY_REG_SIGNOUT)
        {
            bool ret_value_remove = remove_relay(request.request_u.relay_req.request_details_u.signout_request.relay_id);
            retval.responese_details_u.signout_response.status = ret_value_remove;

            if (write_exact(user->fd, &retval, sizeof(relay_req_res_t)) == false)
            {
                fprintf(stderr, "Failed to send full response\n");
                close(user->fd);
                retval.responese_details_u.signout_response.status = false;
            }
            else
            {
                printf("Removed relay with ID: %u\n", request.request_u.relay_req.request_details_u.signout_request.relay_id);
                fflush(stdout);
                close(user->fd);
            }
        }
        else if(request.request_type == CLIENT_REQUEST)
        {
            handle_send_relay(user->fd);
            close(user->fd);
        }
        else if(request.request_type == CLIENT_ROUTE_SELECTED)
        {
            update_relays_load(request.request_u.create_circuit_req.guard_index, 
                               request.request_u.create_circuit_req.middle_index, 
                               request.request_u.create_circuit_req.exit_index);
            close(user->fd);
        }
    }
}

static void* relay_accept_loop_func(void* _)
{   
    (void)(_);
    accept_loop(server_socket_fd, client_callback);
    return NULL;
}

server_running_status_e run_dir_server(const char *config_file)
{
    server_running_status_e retval = running_status_success;
    server_config_metadata_t* config = NULL;

    config = fetch_server_config((char*)config_file);

    if(config == NULL)
    {
        fprintf(stderr, "Failed to do configuration for the server\n");
        retval = running_status_failure;
    }
    else
    {
        simulation_state_init();
        init_relay_manager();

        server_socket_fd = create_and_bind(config);

        if(server_socket_fd == -1)
        {
            fprintf(stderr, "Failed to bind \n");
            retval = running_status_failure;
        }
        else
        {
            if(listen(server_socket_fd,128) < 0)
            {
                perror("listen");
                close(server_socket_fd);
                retval = running_status_failure;
            }
            else
            {
                pthread_t connection_thread_id;
                int con_thread_error = pthread_create(&connection_thread_id, NULL, relay_accept_loop_func, NULL);

                if (con_thread_error != 0)
                {
                    retval = running_status_failure;
                }
                else
                {
                    simulation_log_info("Directory simulation layer initialized");
                    run_commands();

                    shutdown(server_socket_fd, SHUT_RDWR);
                    close(server_socket_fd);
                    server_socket_fd = FAILURE;

                    pthread_join(connection_thread_id, NULL);
                    free_relay_manager();
                }
            }
        }
    }

    if (server_socket_fd != FAILURE)
    {
        close(server_socket_fd);
        server_socket_fd = FAILURE;
    }

    free_server_config(config);

    return retval;
}