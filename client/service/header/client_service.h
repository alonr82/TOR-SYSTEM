#ifndef CLIENT_SERVICE_H
#define CLIENT_SERVICE_H

#include <stdint.h>
#include <stdbool.h>
#include "client_run.h"

#define CLIENT_SERVICE_LINE_SIZE 4096

int run_client_service(void);

#endif