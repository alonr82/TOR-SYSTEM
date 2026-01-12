#include <stdio.h>
#include <stdlib.h>
#include "relay_run_time.h"

int main(int argc, char** argv)
{
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <dir.cfg>\n", argv[0]);
        return 1;
    }

    return run_relay(argv[1]) ? 0 : 1;
}