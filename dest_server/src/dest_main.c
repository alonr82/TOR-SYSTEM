#include "dest_server.h"

int main(void)
{
    printf("Starting Destination Server...\n");
    return run_server() ? 0 : 1;
}