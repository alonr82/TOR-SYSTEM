#include "../common/common.h"

typedef enum 
{
    FAILED_INIT = -1,
    ALREADY_INIT = 1,
    SUCCESS_INIT = 0,
    FAILED_ALLOCATION = -2
} status_init_e;

typedef enum
{
    NOT_INITIALIZED = -2,
    FREE_FAILED = -1,
    FREE_SUCCESS = 0
}free_status_e;

typedef struct 
{
    relay_data_t * data;
    bool exists;
}relay_data_item_t;

/**
 * @brief this function initializes the relay manager
 * 
 * @return status_init_e that indicates the status of the function
 */
status_init_e init_relay_manager();

/**
 * @brief this function frees all the resources used by relay manager
 * 
 * @return free_status_e that indicates the status of the function
 */
free_status_e free_relay_manager();

/**
 * @brief Get the relay object
 * 
 * @param relay_id the id of the relay to get
 * @return relay_data_t* pointer to the requeted relay, NULL if not exists
 */
relay_data_t* get_relay(uint32_t relay_id);

/**
 * @brief this function generates new relay
 * 
 * @return relay_data_t* pointer to the new relay or NULL if there was a problem
 */
relay_data_t* generate_relay();

/**
 * @brief this function removes the relay of the id we got
 * @param relay_id the id of the relay to remove
 * @return true if the relay was removed successfully, false otherwise
 */
bool remove_relay(uint32_t relay_id);

/**
 * @brief this function gets a batch of relay descriptors
 * @param output pointer to the output array of relay descriptors
 * @param start_point pointer to the start point, will be updated to the next start point
 * @param max the maximum number of relay descriptors to get
 * @return uint32_t the number of relay descriptors written to output
 */
uint32_t get_relay_batch(relay_decript_t *output, uint32_t* start_point, uint32_t max);
