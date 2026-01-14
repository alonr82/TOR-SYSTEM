#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "tor_protocol.h"   // נמצא ב-common, וה-INCLUDES כבר כולל -Icommon
#include "create_circuit.h"


int main(int argc, char *argv[])
{
    connect_to_dir_server("config/dir_server_config.cfg");
    return 0;
}
