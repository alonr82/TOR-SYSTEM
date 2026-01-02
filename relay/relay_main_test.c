#define _POSIX_C_SOURCE 200112L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "dir_server_config.h"
#include "sock_utilities.h"
#include "relay_reg.h"  

int main(int argc, char** argv)
{
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <dir.cfg> [n]\n", argv[0]);
        return 1;
    }

    int n = (argc >= 3) ? atoi(argv[2]) : 5;
    if (n <= 0) n = 5;

    server_config_metadata_t* cfg = fetch_server_config((char*)argv[1]);
    if (!cfg) {
        fprintf(stderr, "relay_test: failed to read config\n");
        return 1;
    }

    for (int i = 0; i < n; i++) {
        relay_code_e result = relay_connect_only((char*)argv[1]);
        printf("relay %d connect_only result: %s\n", i+1,
               (result == relay_success) ? "SUCCESS" : "FAILURE");
        
    }
    free_server_config(cfg);
    return 0;
}
