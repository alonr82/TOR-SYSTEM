#include <stdio.h>
#include <stdlib.h>
#include "run_time.h"

int main(int argc, char** argv)
{
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <dir.cfg>\n", argv[0]);
        return 1;
    }

    return (run_dir_server(argv[1]) == running_status_success) ? 0 : 1;
}
