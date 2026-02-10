#ifndef TOR_E2E_CRYPTO_H
#define TOR_E2E_CRYPTO_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <arpa/inet.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/err.h>

#define E2E_KEY_LEN 32
#define E2E_NONCE_LEN 12
#define E2E_TAG_LEN 16
#define E2E_CIPHERTEXT_LEN_LEN 2
#define TOR_OPENSSL_OK  1
#define E2E_BLOB_NONCE_OFFSET 0
#define E2E_BLOB_CIPHERTEXT_LEN_OFFSET (E2E_BLOB_NONCE_OFFSET + E2E_NONCE_LEN)
#define E2E_BLOB_CIPHERTEXT_OFFSET (E2E_BLOB_CIPHERTEXT_LEN_OFFSET + E2E_CIPHERTEXT_LEN_LEN)

/**
 * @brief this function creates a frame contains ciphertext nonce len and tag
 * 
 * @param plaintext the text we want to encrypt
 * @param plaintext_len the len of the text we want to encrypt
 * @param key the key we use for the encryption
 * @param out_frame pointer to the start of the frame we will fill
 * @param out_frame_len the final len of the frame we fill
 * @return true for success
 * @return false otherwise
 */
bool gen_encrypted_framed_message(const uint8_t *plaintext, uint16_t plaintext_len, 
    const uint8_t key[E2E_KEY_LEN], uint8_t *out_frame, uint16_t *out_frame_len);

#endif
