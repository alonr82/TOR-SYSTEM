#ifndef CREATE_CIRCUIT_H
#define CREATE_CIRCUIT_H

#include "tor_protocol.h"
#include "sock_utilities.h"
#include "relay_reg.h"
#include "dir_server_config.h"
#include "tor_crypto.h"
#include <time.h>

#define MAX_RELAY_BATCH_SIZE 10

bool connect_to_dir_server(const char *dir_cfg);

int circuit_connect_guard(circuit_t * circuit);

bool circuit_create(circuit_t *circuit);

bool circuit_extend(circuit_t *circuit);

bool build_default_circuit(circuit_t *circuit, relay_decript_t *relay_list);

bool send_onion_data(circuit_t *circuit, const uint8_t *plaintext, uint16_t plaintext_len);

bool recv_onion_data(circuit_t *circuit, tor_msg_t *in_msg, uint8_t *out_plaintext, uint16_t *out_len);

/* ===== new APIs used by client_main.c ===== */

/*
 * Builds circuit using directory config file, returns ready circuit.
 */
bool build_circuit_from_dir(const char *dir_cfg, circuit_t *out_circuit);

/*
 * Sends a KEYX message (plain) to peer through exit routing envelope.
 * key_len must be E2E_KEY_LEN.
 */
bool chat_send_keyx(circuit_t *circuit,
                    uint32_t peer_ip_v4_nbo,
                    uint16_t peer_port_nbo,
                    const uint8_t key[E2E_KEY_LEN]);

/*
 * Encrypts plaintext with e2e_key into inner DATA, wraps with routing envelope and sends through onion.
 */
bool chat_send_encrypted(circuit_t *circuit,
                         uint32_t peer_ip_v4_nbo,
                         uint16_t peer_port_nbo,
                         const uint8_t e2e_key[E2E_KEY_LEN],
                         const uint8_t *plaintext,
                         uint16_t plaintext_len);

#endif