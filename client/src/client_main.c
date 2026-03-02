#define _POSIX_C_SOURCE 200112L

#include <stdlib.h>
#include "client_run.h"

int main(void)
{
    int retval = EXIT_SUCCESS;
    
    retval = start_client_application();
    
    return retval;
}