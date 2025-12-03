#include "relay_manager.h"

int main(int argc, char *argv[])
{
    init_relay_manager();
    relay_data_t * test1 = generate_relay();
    relay_data_t * test2 = generate_relay();
    relay_data_t * test3 = generate_relay();
    printf("new relay with id: %u\n", test1->relay_id);
    printf("new relay with id: %u\n", test2->relay_id);
    printf("new relay with id: %u\n", test3->relay_id);
    relay_decript_t *arr = calloc(10, sizeof(relay_decript_t));
    uint32_t found = get_relay_batch(arr, &(uint32_t){0},2);
    printf("found %u relays\n", found);
    for(uint32_t i = 0; i < found; i++)
    {
        printf("the relay id is: %u\n", arr[i].relay_id);
    }
    free(arr);
    free_relay_manager();
    return 0;
}