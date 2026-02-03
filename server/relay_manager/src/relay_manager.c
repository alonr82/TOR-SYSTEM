#include "relay_manager.h"
#include <pthread.h>
#include <string.h>

#define MIN_NUM_OF_RELAYS 3
#define STACK_RELAY_MIN_LEN 1

static bool initialized = false;

static uint32_t items_length;
static relay_data_item_t* items;
static uint32_t max_item_index;
static uint32_t stack_length;
static uint32_t* removed_id_stack; 
static uint32_t max_stack_index;

static pthread_mutex_t relay_manager_lock = PTHREAD_MUTEX_INITIALIZER;

status_init_e init_relay_manager()
{
    status_init_e retval = SUCCESS_INIT;
    pthread_mutex_lock(&relay_manager_lock);
    if (initialized)
    {
        pthread_mutex_unlock(&relay_manager_lock);
        retval = ALREADY_INIT;
    }
    if(retval != ALREADY_INIT)
    {
        initialized = true;
        items_length = MIN_NUM_OF_RELAYS;
        items = calloc(items_length, sizeof(relay_data_item_t));
        if(items == NULL)
        {
            retval = FAILED_ALLOCATION;
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
            max_stack_index = 0;
            pthread_mutex_unlock(&relay_manager_lock);
        }
    }
    return retval;
}

free_status_e free_relay_manager()
{
    free_status_e retval = FREE_SUCCESS;
    pthread_mutex_lock(&relay_manager_lock);
    if(!initialized)
    {
        retval = NOT_INITIALIZED;
        pthread_mutex_unlock(&relay_manager_lock);
    }
    if(retval != NOT_INITIALIZED)
    {
        for(uint32_t index = 0; index < items_length; index++)
        {
            if(items[index].exists)
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
        pthread_mutex_unlock(&relay_manager_lock);
    }
    return retval;
}

relay_data_t* get_relay(uint32_t relay_id)
{
    relay_data_t *retval = NULL;
    pthread_mutex_lock(&relay_manager_lock);
    if(initialized && relay_id < items_length && items[relay_id].exists)
    {
        retval = items[relay_id].data;
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
    pthread_mutex_lock(&relay_manager_lock);
    if(max_stack_index == 0)
    {
        if(items_length <= max_item_index)
        {
            uint32_t old_length = items_length;
            items_length*=2;
            relay_data_item_t* new_items = realloc(items, items_length * sizeof(relay_data_item_t));
            if(new_items != NULL)
            {
                memset(new_items + old_length, 0, (items_length - old_length) * sizeof(relay_data_item_t));
                items = new_items;

            }
            else
            {
                pthread_mutex_unlock(&relay_manager_lock);
                return retval;
            }
        }
        memset(&items[max_item_index], 0, sizeof(relay_data_item_t));
        items[max_item_index].data = calloc(1, sizeof(relay_data_t));
        items[max_item_index].data->relay_id = max_item_index;
        items[max_item_index].data->descriptor.relay_id = max_item_index;
        items[max_item_index].exists = true;
        items[max_item_index].data->descriptor.relay_port = relay_info->relay_port;
        items[max_item_index].data->descriptor.ip_type = relay_info->ip_type;
        fetch_ip_address(items[max_item_index].data->descriptor.relay_ip, relay_info->relay_ip, relay_info->ip_type);
        retval = items[max_item_index++].data;
        pthread_mutex_unlock(&relay_manager_lock);
    }
    else
    {
        max_stack_index--;
        uint32_t used_id = removed_id_stack[max_stack_index];
        memset(&items[used_id], 0, sizeof(items[used_id]));
        items[used_id].data = calloc(1, sizeof(relay_data_t));
        items[used_id].data->relay_id = used_id;
        items[used_id].data->descriptor.relay_id = used_id;
        items[used_id].data->descriptor.relay_port = relay_info->relay_port;
        items[used_id].data->descriptor.ip_type = relay_info->ip_type;
        fetch_ip_address(items[used_id].data->descriptor.relay_ip, relay_info->relay_ip, relay_info->ip_type);
        items[used_id].exists = true;
        retval = items[used_id].data;
        pthread_mutex_unlock(&relay_manager_lock);
    }
    return retval;
}
bool remove_relay(uint32_t relay_id)
{
    bool retval = false;
    pthread_mutex_lock(&relay_manager_lock);
    if(initialized && relay_id < items_length && items[relay_id].exists)
    {
        free(items[relay_id].data);
        items[relay_id].data = NULL;
        items[relay_id].exists = false;
        if(stack_length <= max_stack_index)
        {
            uint32_t old_len = stack_length;
            stack_length *= 2;
            uint32_t* new_stack = realloc(removed_id_stack, stack_length * sizeof(uint32_t));
            if(new_stack == NULL)
            {
                retval = false;
            }
            else
            {
                removed_id_stack = new_stack;
                memset(removed_id_stack + old_len, 0, (stack_length - old_len) * sizeof(uint32_t));
                removed_id_stack[max_stack_index++] = relay_id;
                retval = true;
            }
        }
        else
        {
            removed_id_stack[max_stack_index++] = relay_id;
            retval = true;
        }
    }
    pthread_mutex_unlock(&relay_manager_lock);
    return retval; 
}

uint32_t get_relay_batch(relay_decript_t* out, uint32_t* start, uint32_t max) 
{
    pthread_mutex_lock(&relay_manager_lock);
    uint32_t found = 0;
    uint32_t index = *start % items_length;
    uint32_t count = 0;
    while (found < max && count < items_length) 
    {
        if (items[index].exists) 
        {
            memcpy(&out[found++], &items[index].data->descriptor, sizeof(relay_decript_t));
        }
        index = (index + 1) % items_length;
        count++;
    }
    *start = index;
    pthread_mutex_unlock(&relay_manager_lock);
    return found;
}