#ifndef FORWARD_MSG_H
#define FORWARD_MSG_H

#include <stdint.h>
#include <stdbool.h>
#include <poll.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdarg.h>

#include "tor_protocol.h"
#include "sock_utilities.h"
#include "simulation_state.h"

#define TWO_SOCKETS 2

typedef enum
{
    relay_runtime_mode_normal = 0,
    relay_runtime_mode_attacker_controlled
}relay_runtime_mode_e;

extern relay_runtime_mode_e g_relay_runtime_mode;

typedef struct {
    int last_fd;
    int next_fd;
    bool hop_key_ready;

    bool dest_ready;
    uint32_t dest_ip_v4;    /* network byte order */
    uint16_t dest_port;     /* network byte order */

    uint8_t hop_key[E2E_KEY_LEN];
} session_t;

bool process_create_handshake(session_t *session, tor_msg_t *in_msg, tor_msg_t *out_msg, const uint8_t *priv_seed);

void forward_messages(session_t *session, volatile bool *run_flag);

/*
 * connect to peer (client) by ip/port (network byte order)
 */
int connect_to_peer(uint32_t ip_v4_nbo, uint16_t port_nbo);

#endif