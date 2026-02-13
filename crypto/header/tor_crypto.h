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
#include <openssl/kdf.h>
#include <openssl/kdferr.h>

#define E2E_KEY_LEN 32
#define E2E_NONCE_LEN 12
#define E2E_TAG_LEN 16
#define E2E_CIPHERTEXT_LEN_LEN 2
#define TOR_OPENSSL_OK  1
#define E2E_BLOB_NONCE_OFFSET 0
#define E2E_BLOB_CIPHERTEXT_LEN_OFFSET (E2E_BLOB_NONCE_OFFSET + E2E_NONCE_LEN)
#define E2E_BLOB_CIPHERTEXT_OFFSET (E2E_BLOB_CIPHERTEXT_LEN_OFFSET + E2E_CIPHERTEXT_LEN_LEN)

#define TOR_X25519_KEY_LEN 32
#define TOR_SHARED_SECRET_LEN 32
#define TOR_E2E_KEY_LEN 32
#define TOR_HKDF_INFO_MAX_LEN 64

/**
 * @brief this function prints info about openssl library functions failures
 * 
 * @param where 
 */
void tor_crypto_print_openssl_error(const char* where);

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

/**
 * @brief this function decrypts a blob
 * 
 * @param blob the whole frame we got
 * @param blob_len the len of the frame we got
 * @param key the key used to the encryption
 * @param plaintext pointer to the plaintext we will produce
 * @param plaintext_len the len of the plain text
 * @return true 
 * @return false 
 */
bool decrypt_blob(const uint8_t *blob, uint16_t *blob_len, const uint8_t key[E2E_KEY_LEN],
    uint8_t *plaintext, uint16_t *plaintext_len);

/**
 * @brief this function generates empherial pair of keys
 * 
 * @param empherial_public_key 
 * @param empherial_private_key 
 * @return true 
 * @return false 
 */
bool generate_empherial_keypair(uint8_t empherial_public_key[TOR_X25519_KEY_LEN],
    uint8_t empherial_private_key[TOR_X25519_KEY_LEN]);


/**
 * @brief this function derives(calculates) a shared secret between two destinations
 * 
 * @param shared_secret the shared secret we got from the function
 * @param own_private_key 
 * @param partner_public_key 
 * @return true 
 * @return false 
 */
bool derive_shared_secret(uint8_t shared_secret[TOR_SHARED_SECRET_LEN],
    const uint8_t own_private_key[TOR_X25519_KEY_LEN], const uint8_t partner_public_key[TOR_E2E_KEY_LEN]);
    
#endif
