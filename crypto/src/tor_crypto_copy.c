#define _POSIX_C_SOURCE 200112L

#include "tor_crypto.h"

#include <string.h>
#include <stdio.h>
#include <arpa/inet.h>

#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/err.h>

#define TOR_OPENSSL_OK  1

static void tor_crypto_print_openssl_error(const char* where)
{
    unsigned long err_code = 0;
    char error_text[256];

    err_code = ERR_get_error();
    if (err_code == 0)
    {
        fprintf(stderr, "[E2E_CRYPTO] %s: OpenSSL error (unknown)\n", where);
    }
    else
    {
        ERR_error_string_n(err_code, error_text, sizeof(error_text));
        fprintf(stderr, "[E2E_CRYPTO] %s: %s\n", where, error_text);
    }
}

int tor_e2e_random_bytes(uint8_t* out_bytes, size_t out_len)
{
    int retval = 0;

    if (out_bytes != NULL && out_len > 0)
    {
        if (RAND_bytes(out_bytes, (int)out_len) == TOR_OPENSSL_OK)
        {
            retval = 1;
        }
        else
        {
            tor_crypto_print_openssl_error("RAND_bytes");
        }
    }

    return retval;
}

static int tor_e2e_encrypt_internal(const uint8_t e2e_key[E2E_KEY_LEN],const uint8_t nonce[E2E_NONCE_LEN],
    const uint8_t* plaintext, uint16_t plaintext_len,uint8_t* out_ciphertext, uint16_t* out_ciphertext_len,
    uint8_t out_tag[E2E_TAG_LEN])
{
    int retval = 0;
    EVP_CIPHER_CTX* ctx = NULL;
    int produced_len = 0;
    int total_len = 0;
    if (e2e_key != NULL && nonce != NULL && out_ciphertext != NULL && out_ciphertext_len != NULL && out_tag != NULL)
    {
        retval = 1;

        ctx = EVP_CIPHER_CTX_new();
        if (ctx == NULL)
        {
            tor_crypto_print_openssl_error("EVP_CIPHER_CTX_new");
            retval = 0;
        }

        if (retval == 1)
        {
            if (EVP_EncryptInit_ex(ctx, EVP_chacha20_poly1305(), NULL, NULL, NULL) != TOR_OPENSSL_OK)
            {
                tor_crypto_print_openssl_error("EVP_EncryptInit_ex(algo)");
                retval = 0;
            }
        }

        if (retval == 1)
        {
            if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_IVLEN, E2E_NONCE_LEN, NULL) != TOR_OPENSSL_OK)
            {
                tor_crypto_print_openssl_error("EVP_CTRL_AEAD_SET_IVLEN");
                retval = 0;
            }
        }

        if (retval == 1)
        {
            if (EVP_EncryptInit_ex(ctx, NULL, NULL, e2e_key, nonce) != TOR_OPENSSL_OK)
            {
                tor_crypto_print_openssl_error("EVP_EncryptInit_ex(key,nonce)");
                retval = 0;
            }
        }

        if (retval == 1)
        {
            if (plaintext_len > 0)
            {
                if (plaintext == NULL)
                {
                    retval = 0;
                }
                else
                {
                    if (EVP_EncryptUpdate(ctx, out_ciphertext, &produced_len, plaintext, (int)plaintext_len) != TOR_OPENSSL_OK)
                    {
                        tor_crypto_print_openssl_error("EVP_EncryptUpdate");
                        retval = 0;
                    }
                    else
                    {
                        total_len = produced_len;
                    }
                }
            }
        }

        if (retval == 1)
        {
            if (EVP_EncryptFinal_ex(ctx, out_ciphertext + total_len, &produced_len) != TOR_OPENSSL_OK)
            {
                tor_crypto_print_openssl_error("EVP_EncryptFinal_ex");
                retval = 0;
            }
            else
            {
                total_len += produced_len;
            }
        }

        if (retval == 1)
        {
            if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_GET_TAG, E2E_TAG_LEN, out_tag) != TOR_OPENSSL_OK)
            {
                tor_crypto_print_openssl_error("EVP_CTRL_AEAD_GET_TAG");
                retval = 0;
            }
        }

        if (retval == 1)
        {
            *out_ciphertext_len = (uint16_t)total_len;
        }
    }

    if (ctx != NULL)
    {
        EVP_CIPHER_CTX_free(ctx);
    }

    return retval;
}

static int tor_e2e_decrypt_internal(const uint8_t e2e_key[E2E_KEY_LEN],const uint8_t nonce[E2E_NONCE_LEN],
    const uint8_t* ciphertext, uint16_t ciphertext_len,const uint8_t tag[E2E_TAG_LEN],
    uint8_t* out_plaintext, uint16_t* out_plaintext_len)
{
    int retval = 0;

    EVP_CIPHER_CTX* ctx = NULL;
    int produced_len = 0;
    int total_len = 0;

    if (e2e_key != NULL && nonce != NULL && tag != NULL && out_plaintext != NULL && out_plaintext_len != NULL)
    {
        retval = 1;

        ctx = EVP_CIPHER_CTX_new();
        if (ctx == NULL)
        {
            tor_crypto_print_openssl_error("EVP_CIPHER_CTX_new");
            retval = 0;
        }

        if (retval == 1)
        {
            if (EVP_DecryptInit_ex(ctx, EVP_chacha20_poly1305(), NULL, NULL, NULL) != TOR_OPENSSL_OK)
            {
                tor_crypto_print_openssl_error("EVP_DecryptInit_ex(algo)");
                retval = 0;
            }
        }

        if (retval == 1)
        {
            if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_IVLEN, E2E_NONCE_LEN, NULL) != TOR_OPENSSL_OK)
            {
                tor_crypto_print_openssl_error("EVP_CTRL_AEAD_SET_IVLEN");
                retval = 0;
            }
        }

        if (retval == 1)
        {
            if (EVP_DecryptInit_ex(ctx, NULL, NULL, e2e_key, nonce) != TOR_OPENSSL_OK)
            {
                tor_crypto_print_openssl_error("EVP_DecryptInit_ex(key,nonce)");
                retval = 0;
            }
        }

        if (retval == 1)
        {
            if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_TAG, E2E_TAG_LEN, (void*)tag) != TOR_OPENSSL_OK)
            {
                tor_crypto_print_openssl_error("EVP_CTRL_AEAD_SET_TAG");
                retval = 0;
            }
        }

        if (retval == 1)
        {
            if (ciphertext_len > 0)
            {
                if (ciphertext == NULL)
                {
                    retval = 0;
                }
                else
                {
                    if (EVP_DecryptUpdate(ctx, out_plaintext, &produced_len, ciphertext, (int)ciphertext_len) != TOR_OPENSSL_OK)
                    {
                        tor_crypto_print_openssl_error("EVP_DecryptUpdate");
                        retval = 0;
                    }
                    else
                    {
                        total_len = produced_len;
                    }
                }
            }
        }

        if (retval == 1)
        {
            if (EVP_DecryptFinal_ex(ctx, out_plaintext + total_len, &produced_len) != TOR_OPENSSL_OK)
            {
                retval = 0;
            }
            else
            {
                total_len += produced_len;
                *out_plaintext_len = (uint16_t)total_len;
                retval = 1;
            }
        }
    }

    if (ctx != NULL)
    {
        EVP_CIPHER_CTX_free(ctx);
    }

    return retval;
}

static void tor_e2e_blob_write_ciphertext_len(uint8_t* out_blob, uint16_t ciphertext_len)
{
    uint16_t ct_len_net = 0;

    ct_len_net = htons(ciphertext_len);
    memcpy(out_blob + E2E_BLOB_CIPHERTEXT_LEN_OFFSET, &ct_len_net, E2E_CIPHERTEXT_LEN_LEN);
}

static uint16_t tor_e2e_blob_read_ciphertext_len(const uint8_t* blob)
{
    uint16_t retval = 0;
    uint16_t ct_len_net = 0;

    memcpy(&ct_len_net, blob + E2E_BLOB_CIPHERTEXT_LEN_OFFSET, E2E_CIPHERTEXT_LEN_LEN);
    retval = ntohs(ct_len_net);

    return retval;
}

static uint16_t tor_e2e_blob_total_len(uint16_t ciphertext_len)
{
    uint16_t retval = 0;

    retval = (uint16_t)(TOR_E2E_BLOB_CIPHERTEXT_OFFSET + ciphertext_len + E2E_TAG_LEN);

    return retval;
}

int tor_e2e_encrypt_blob_chacha20poly1305(const uint8_t e2e_key[E2E_KEY_LEN],
    const uint8_t* plaintext, uint16_t plaintext_len,uint8_t* out_blob, uint16_t* out_blob_len)
{
    int retval = 0;

    uint8_t nonce[E2E_NONCE_LEN];
    uint8_t tag[E2E_TAG_LEN];

    uint16_t ciphertext_len = 0;
    uint16_t total_len = 0;

    uint8_t* ciphertext_ptr = NULL;
    uint8_t* tag_ptr = NULL;

    if (e2e_key != NULL && out_blob != NULL && out_blob_len != NULL)
    {
        retval = 1;

        if (retval == 1)
        {
            if (tor_e2e_random_bytes(nonce, E2E_NONCE_LEN) != 1)
            {
                retval = 0;
            }
        }

        if (retval == 1)
        {
            ciphertext_ptr = out_blob + TOR_E2E_BLOB_CIPHERTEXT_OFFSET;

            if (tor_e2e_encrypt_internal(e2e_key, nonce, plaintext, plaintext_len, ciphertext_ptr, &ciphertext_len, tag) != 1)
            {
                retval = 0;
            }
        }

        if (retval == 1)
        {
            total_len = tor_e2e_blob_total_len(ciphertext_len);

            memcpy(out_blob + E2E_BLOB_NONCE_OFFSET, nonce, E2E_NONCE_LEN);
            tor_e2e_blob_write_ciphertext_len(out_blob, ciphertext_len);

            tag_ptr = out_blob + (TOR_E2E_BLOB_CIPHERTEXT_OFFSET + ciphertext_len);
            memcpy(tag_ptr, tag, E2E_TAG_LEN);

            *out_blob_len = total_len;
            retval = 1;
        }
    }

    return retval;
}

int tor_e2e_decrypt_blob_chacha20poly1305(const uint8_t e2e_key[E2E_KEY_LEN],const uint8_t* blob, uint16_t blob_len,
    uint8_t* out_plaintext, uint16_t* out_plaintext_len)
{
    int retval = 0;

    uint8_t nonce[E2E_NONCE_LEN];

    uint16_t ciphertext_len = 0;
    uint16_t expected_total = 0;

    const uint8_t* ciphertext_ptr = NULL;
    const uint8_t* tag_ptr = NULL;

    uint16_t produced_plaintext_len = 0;

    if (e2e_key != NULL && blob != NULL && out_plaintext != NULL && out_plaintext_len != NULL)
    {
        retval = 1;

        if (retval == 1)
        {
            if (blob_len < TOR_E2E_BLOB_MIN_LEN)
            {
                retval = 0;
            }
        }

        if (retval == 1)
        {
            memcpy(nonce, blob + E2E_BLOB_NONCE_OFFSET, E2E_NONCE_LEN);

            ciphertext_len = tor_e2e_blob_read_ciphertext_len(blob);
            expected_total = tor_e2e_blob_total_len(ciphertext_len);

            if (expected_total != blob_len)
            {
                retval = 0;
            }
        }

        if (retval == 1)
        {
            ciphertext_ptr = blob + TOR_E2E_BLOB_CIPHERTEXT_OFFSET;
            tag_ptr = blob + (TOR_E2E_BLOB_CIPHERTEXT_OFFSET + ciphertext_len);

            if (tor_e2e_decrypt_internal(e2e_key, nonce, ciphertext_ptr, ciphertext_len, tag_ptr, out_plaintext, &produced_plaintext_len) != 1)
            {
                retval = 0;
            }
            else
            {
                *out_plaintext_len = produced_plaintext_len;
                retval = 1;
            }
        }
    }

    return retval;
}
