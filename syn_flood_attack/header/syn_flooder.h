#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <errno.h>
#include <netinet/tcp.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <time.h>
#include <stdbool.h>
#include "create_circuit.h"


#define DIR_SERVER_CFG_PATH "config/dir_server_config.cfg"
#define CARRY_BITS 16
#define PAIR_OF_BYTES 2
#define INIT_SUM 0
#define LAST_BYTE 1
#define RESET 0
#define LOW_SIXTEEN_MASK 0xFFFF
#define IP_MAX_POSSIBBLE 256
#define IPV4 4
#define TIME_TO_LIVE 255
#define IP_HEADER_LEN 5
#define MAX_POSSIBLE_ID 65535
#define FLAG_OFF 0
#define FLAG_ON 1
#define REGULAR_PRIORITY 0
#define SAVED_PORTS 1024
#define INIT_SEQ 0
#define DATA_OFFSET 5 
#define WINDOW_SIZE 5840
#define IP_BUFFER_LEN 20
#define DATAGRAM_SIZE 4096

typedef struct __attribute__((packed))
{
    uint32_t source_addr;
    uint32_t dest_addr;
    uint8_t place_holder;
    uint8_t  protocol;
    uint16_t tcp_len;
}pseudo_header_t;

