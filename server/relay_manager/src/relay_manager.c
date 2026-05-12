#include "relay_manager.h"
#include <pthread.h>
#include <string.h>
#include <stdlib.h>

#define MIN_NUM_OF_RELAYS 3
#define STACK_RELAY_MIN_LEN 1
#define TRUST_SCORE_VERIFIED_LOCAL 90
#define TRUST_SCORE_NEW_REMOTE 80
#define TRUST_DUPLICATE_SOURCE_PENALTY 20
#define TRUST_SCORE_MIN_REMOTE 20

static bool initialized = false;
static uint32_t items_length;
static relay_data_item_t* items;
static uint32_t max_item_index;
static uint32_t stack_length;
static uint32_t* removed_id_stack; 
static uint32_t max_stack_index;
static pthread_mutex_t relay_manager_lock = PTHREAD_MUTEX_INITIALIZER;

static bool is_loopback_ipv4(const uint8_t ip[IP4_SIZE])
{
    bool retval = false;

    if(ip != NULL)
    {
        if(ip[0] == 127)
        {
            retval = true;
        }
    }

    return retval;
}

static uint32_t ipv4_bytes_to_u32(const uint8_t ip[IP4_SIZE])
{
    uint32_t retval = 0;

    if(ip != NULL)
    {
        retval =
            ((uint32_t)ip[0] << 24) |
            ((uint32_t)ip[1] << 16) |
            ((uint32_t)ip[2] << 8)  |
            ((uint32_t)ip[3]);
    }

    return retval;
}

static uint32_t count_relays_from_same_source_locked(const relay_decript_t *relay_info)
{
    uint32_t retval = 0;

    if(relay_info != NULL && relay_info->ip_type == 4)
    {
        uint32_t source_ip = ipv4_bytes_to_u32(relay_info->relay_ip);

        for(uint32_t index = 0; index < items_length; index++)
        {
            if(items[index].exists == true)
            {
                if(items[index].data != NULL)
                {
                    if(items[index].data->descriptor.ip_type == 4)
                    {
                        if(items[index].data->descriptor.source_ip_v4 == source_ip)
                        {
                            retval++;
                        }
                    }
                }
            }
        }
    }

    return retval;
}

static uint32_t calc_initial_trust_score(const relay_decript_t *relay_info, uint32_t same_source_count)
{
    uint32_t retval = TRUST_SCORE_NEW_REMOTE;

    if(relay_info != NULL)
    {
        if(relay_info->ip_type == 4)
        {
            if(is_loopback_ipv4(relay_info->relay_ip) == true)
            {
                retval = TRUST_SCORE_VERIFIED_LOCAL;
            }
            else
            {
                retval = TRUST_SCORE_NEW_REMOTE;

                if(same_source_count > 3)
                {
                    uint32_t penalty = (same_source_count - 3) * TRUST_DUPLICATE_SOURCE_PENALTY;

                    if(penalty >= (TRUST_SCORE_NEW_REMOTE - TRUST_SCORE_MIN_REMOTE))
                    {
                        retval = TRUST_SCORE_MIN_REMOTE;
                    }
                    else
                    {
                        retval = TRUST_SCORE_NEW_REMOTE - penalty;
                    }
                }
            }
        }
        else
        {
            retval = TRUST_SCORE_NEW_REMOTE;
        }
    }

    return retval;
}

status_init_e init_relay_manager()
{
    status_init_e retval = SUCCESS_INIT;

    pthread_mutex_lock(&relay_manager_lock);

    if (initialized)
    {
        retval = ALREADY_INIT;
    }
    else
    {
        initialized = true;
        items_length = MIN_NUM_OF_RELAYS;
        items = calloc(items_length, sizeof(relay_data_item_t));

        if(items == NULL)
        {
            retval = FAILED_ALLOCATION;
            initialized = false;
        }
        else
        {
            max_item_index = 0;

            for(uint32_t index = 0; index < items_length; index++)
            {
                items[index].exists = false;
            }

            stack_length = STACK_RELAY_MIN_LEN;
            removed_id_stack = calloc(stack_length,sizeof(uint32_t));

            if(removed_id_stack == NULL)
            {
                free(items);
                items = NULL;
                items_length = 0;
                initialized = false;
                retval = FAILED_ALLOCATION;
            }
            else
            {
                max_stack_index = 0;
            }
        }
    }

    pthread_mutex_unlock(&relay_manager_lock);

    return retval;
}

free_status_e free_relay_manager()
{
    free_status_e retval = FREE_SUCCESS;

    pthread_mutex_lock(&relay_manager_lock);

    if(!initialized)
    {
        retval = NOT_INITIALIZED;
    }
    else
    {
        for(uint32_t index = 0; index < items_length; index++)
        {
            if(items[index].exists == true)
            {
                free(items[index].data);
                items[index].data = NULL;
                items[index].exists = false;
            }
        }

        free(items);
        items = NULL;

        free(removed_id_stack);
        removed_id_stack = NULL;

        items_length = 0;
        max_item_index = 0;
        stack_length = 0;
        max_stack_index = 0;
        initialized = false;
    }

    pthread_mutex_unlock(&relay_manager_lock);

    return retval;
}

relay_data_t* get_relay(uint32_t relay_id)
{
    relay_data_t *retval = NULL;

    pthread_mutex_lock(&relay_manager_lock);

    if(initialized == true)
    {
        if(relay_id < items_length)
        {
            if(items[relay_id].exists == true)
            {
                retval = items[relay_id].data;
            }
        }
    }

    pthread_mutex_unlock(&relay_manager_lock);

    return retval;
}

bool fetch_ip_address(uint8_t* dest, uint8_t* src, uint8_t ip_type)
{
    bool retval = false;

    if(ip_type == 4)
    {
        memset(dest, 0, IP6_SIZE);
        memcpy(dest, src, IP4_SIZE);  
        retval = true;
    }
    else if(ip_type == 6)
    {
        memcpy(dest, src, IP6_SIZE);
        retval = true;
    }

    return retval;
}

relay_data_t* generate_relay(relay_decript_t *relay_info)
{
    relay_data_t* retval = NULL;
    uint32_t same_source_count = 1;
    uint32_t initial_trust_score = TRUST_SCORE_NEW_REMOTE;
    uint32_t used_id = 0;

    pthread_mutex_lock(&relay_manager_lock);

    if(relay_info != NULL)
    {
        same_source_count = count_relays_from_same_source_locked(relay_info) + 1;
        initial_trust_score = calc_initial_trust_score(relay_info, same_source_count);

        if(max_stack_index == 0)
        {
            if(items_length <= max_item_index)
            {
                uint32_t old_length = items_length;
                relay_data_item_t* new_items = NULL;

                items_length *= 2;
                new_items = realloc(items, items_length * sizeof(relay_data_item_t));

                if(new_items != NULL)
                {
                    memset(new_items + old_length, 0, (items_length - old_length) * sizeof(relay_data_item_t));
                    items = new_items;
                }
                else
                {
                    items_length = old_length;
                }
            }

            if(items_length > max_item_index)
            {
                used_id = max_item_index;
                max_item_index++;
            }
            else
            {
                used_id = UINT32_MAX;
            }
        }
        else
        {
            max_stack_index--;
            used_id = removed_id_stack[max_stack_index];
        }

        if(used_id != UINT32_MAX)
        {
            memset(&items[used_id], 0, sizeof(relay_data_item_t));
            items[used_id].data = calloc(1, sizeof(relay_data_t));

            if(items[used_id].data != NULL)
            {
                items[used_id].data->relay_id = used_id;
                items[used_id].data->descriptor.relay_id = used_id;
                items[used_id].data->descriptor.relay_port = relay_info->relay_port;
                items[used_id].data->descriptor.ip_type = relay_info->ip_type;
                items[used_id].data->descriptor.is_active = relay_info->is_active;
                items[used_id].data->descriptor.assigned_count = 0;
                items[used_id].data->descriptor.is_malicious = relay_info->is_malicious;
                items[used_id].data->descriptor.is_verified_historically = false;
                items[used_id].data->descriptor.is_filtered = false;
                items[used_id].data->descriptor.eligible_guard = false;
                items[used_id].data->descriptor.is_guard = false;
                items[used_id].data->descriptor.is_exit = true;
                items[used_id].data->descriptor.trust_score = initial_trust_score;
                items[used_id].data->descriptor.same_source_count = same_source_count;
                items[used_id].data->descriptor.registration_time = relay_info->registration_time;
                items[used_id].data->descriptor.uptime_seconds = relay_info->uptime_seconds;
                items[used_id].data->descriptor.first_seen_unix_sec = relay_info->first_seen_unix_sec;

                fetch_ip_address(items[used_id].data->descriptor.relay_ip, relay_info->relay_ip, relay_info->ip_type);
                memcpy(items[used_id].data->descriptor.identify_pub, relay_info->identify_pub, TOR_ID_PUB_LEN);

                if(relay_info->ip_type == 4)
                {
                    items[used_id].data->descriptor.source_ip_v4 = ipv4_bytes_to_u32(relay_info->relay_ip);
                }
                else
                {
                    items[used_id].data->descriptor.source_ip_v4 = 0;
                }

                items[used_id].exists = true;
                retval = items[used_id].data;
            }
        }
    }

    pthread_mutex_unlock(&relay_manager_lock);

    return retval;
}

bool remove_relay(uint32_t relay_id)
{
    bool retval = false;

    pthread_mutex_lock(&relay_manager_lock);

    if(initialized == true)
    {
        if(relay_id < items_length)
        {
            if(items[relay_id].exists == true)
            {
                free(items[relay_id].data);
                items[relay_id].data = NULL;
                items[relay_id].exists = false;

                if(stack_length <= max_stack_index)
                {
                    uint32_t old_len = stack_length;
                    uint32_t* new_stack = NULL;

                    stack_length *= 2;
                    new_stack = realloc(removed_id_stack, stack_length * sizeof(uint32_t));

                    if(new_stack != NULL)
                    {
                        removed_id_stack = new_stack;
                        memset(removed_id_stack + old_len, 0, (stack_length - old_len) * sizeof(uint32_t));
                    }
                    else
                    {
                        stack_length = old_len;
                    }
                }

                if(stack_length > max_stack_index)
                {
                    removed_id_stack[max_stack_index] = relay_id;
                    max_stack_index++;
                    retval = true;
                }
            }
        }
    }

    pthread_mutex_unlock(&relay_manager_lock);

    return retval; 
}

static int compare_relays(const void *a, const void *b) 
{
    relay_data_item_t *ra = *(relay_data_item_t **)a;
    relay_data_item_t *rb = *(relay_data_item_t **)b;
    int retval = 0;

    if (ra->data->descriptor.trust_score > rb->data->descriptor.trust_score)
    {
        retval = -1;
    }
    else if (ra->data->descriptor.trust_score < rb->data->descriptor.trust_score)
    {
        retval = 1;
    }
    else if (ra->data->descriptor.assigned_count < rb->data->descriptor.assigned_count)
    {
        retval = -1;
    }
    else if (ra->data->descriptor.assigned_count > rb->data->descriptor.assigned_count)
    {
        retval = 1;
    }
    else if (ra->data->descriptor.is_malicious < rb->data->descriptor.is_malicious)
    {
        retval = -1;
    }
    else if (ra->data->descriptor.is_malicious > rb->data->descriptor.is_malicious)
    {
        retval = 1;
    }

    return retval;
}

uint32_t get_relay_batch(relay_decript_t* out, uint32_t* start, uint32_t max) 
{
    uint32_t found = 0;
    relay_data_item_t **active_relays = NULL;
    uint32_t active_count = 0;

    pthread_mutex_lock(&relay_manager_lock);
    
    active_relays = malloc(items_length * sizeof(relay_data_item_t*));
    
    if (active_relays != NULL)
    {
        for (uint32_t i = 0; i < items_length; i++) 
        {
            if (items[i].exists == true) 
            {
                active_relays[active_count] = &items[i];
                active_count++;
            }
        }

        if (active_count > 1) 
        {
            qsort(active_relays, active_count, sizeof(relay_data_item_t*), compare_relays);
        }

        for (uint32_t i = 0; i < active_count && i < max; i++) 
        {
            memcpy(&out[found], &active_relays[i]->data->descriptor, sizeof(relay_decript_t));
            found++;
        }

        free(active_relays);
    }

    *start = 0; 

    pthread_mutex_unlock(&relay_manager_lock);

    return found;
}

void update_relays_load(uint32_t id1, uint32_t id2, uint32_t id3)
{
    pthread_mutex_lock(&relay_manager_lock);

    if (initialized == true) 
    {
        if (id1 < items_length && items[id1].exists == true)
        {
            items[id1].data->descriptor.assigned_count++;
        }

        if (id2 < items_length && items[id2].exists == true)
        {
            items[id2].data->descriptor.assigned_count++;
        }

        if (id3 < items_length && items[id3].exists == true)
        {
            items[id3].data->descriptor.assigned_count++;
        }
    }

    pthread_mutex_unlock(&relay_manager_lock);
}