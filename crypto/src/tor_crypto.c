#include "tor_crypto.h"

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

static bool init_context_framework(EVP_CIPHER_CTX *context, const uint8_t key[E2E_KEY_LEN], 
    const uint8_t nonce[E2E_NONCE_LEN])
{
    bool retval = true;
    if(EVP_EncryptInit_ex(context,EVP_chacha20_poly1305(),NULL,NULL,NULL) != TOR_OPENSSL_OK)
    {
        tor_crypto_print_openssl_error("EVP_EncryptInit_ex(algorithem)");
        retval = false;
    }
    else
    {
        if(EVP_CIPHER_CTX_ctrl(context,EVP_CTRL_AEAD_SET_IVLEN,E2E_NONCE_LEN,NULL) != TOR_OPENSSL_OK)
        {
            tor_crypto_print_openssl_error("EVP_CIPHER_CTX_ctrl");
            retval = false;
        }
        else
        {
            if(EVP_EncryptInit_ex(context,NULL,NULL,key,nonce) != TOR_OPENSSL_OK)
            {
                tor_crypto_print_openssl_error("EVP_EncryptInit_ex(key)");
                retval = false;
            }
        }
    }
    return retval;
}

static bool init_context(EVP_CIPHER_CTX *context, const uint8_t key[E2E_KEY_LEN], 
    const uint8_t nonce[E2E_NONCE_LEN])
{
    bool retval = true;
    if(!(init_context_framework(context,key,nonce)))
    {
        retval = false;
    }
    return retval;
}

static bool update_final(EVP_CIPHER_CTX *context, uint8_t *cipher_text, int *produced_len,
    const uint8_t *plaintext, uint16_t plaintext_len, int *total_len, uint8_t tag[E2E_TAG_LEN])
{
    bool retval = true;
    if(EVP_EncryptUpdate(context,cipher_text,produced_len,plaintext,(int)plaintext_len) != TOR_OPENSSL_OK)
    {
        tor_crypto_print_openssl_error("EVP_EncryptUpdate");
        retval = false;
    }
    else
    {
        *total_len = *produced_len;
        if(EVP_EncryptFinal_ex(context,cipher_text + (*total_len) ,produced_len) != TOR_OPENSSL_OK)
        {
            tor_crypto_print_openssl_error("EVP_EncryptFinal_ex");
            retval = false;
        }
        else
        {
            *total_len += *produced_len;
            if(EVP_CIPHER_CTX_ctrl(context,EVP_CTRL_AEAD_GET_TAG,E2E_TAG_LEN,tag) != TOR_OPENSSL_OK)
            {
                printf("EVP_CIPHER_CTX_ctrl(tag)");
                retval = false;
            }
        }
    }
    return retval;
}

static bool e2e_encrypt(const uint8_t key[E2E_KEY_LEN], const uint8_t nonce[E2E_NONCE_LEN],
    const uint8_t* plaintext, uint16_t plaintext_len, uint8_t *cipher_text, uint16_t* cipher_text_len,
    uint8_t tag[E2E_TAG_LEN])
{
    bool retval = true;
    if(key != NULL && plaintext != NULL && cipher_text != NULL && cipher_text_len != NULL)
    {
        EVP_CIPHER_CTX *context = NULL;
        int produced_len = 0;
        int total_len = 0;
        context = EVP_CIPHER_CTX_new();
        if(context == NULL)
        {
            tor_crypto_print_openssl_error("EVP_CIPHER_CTX_new");
            retval = false;
        }
        else
        {
            if(!(init_context(context,key,nonce)))
            {
                printf("init_context failed:\n");
                retval = false;
                EVP_CIPHER_CTX_free(context);
            }
            else
            {
                if(!(update_final(context,cipher_text,&produced_len,plaintext,plaintext_len,&total_len,tag)))
                {
                    printf("update_final failed:\n");
                    retval = false;
                }
                else
                {
                    *cipher_text_len = (uint16_t) total_len;
                }
                EVP_CIPHER_CTX_free(context);
            }
        }
    }
    else
    {
        retval = false;
    }
    return retval;
}

static uint16_t frame_total_len(uint16_t ciphertext_len)
{
    uint16_t retval = 0;
    retval = (uint16_t) E2E_NONCE_LEN + E2E_CIPHERTEXT_LEN_LEN + ciphertext_len + E2E_TAG_LEN;
    return retval;
}

static void write_frame_len(uint8_t* out_frame, uint16_t ciphertext_len)
{
    uint16_t htons_len = 0;
    htons_len = htons(ciphertext_len);
    memcpy(out_frame + E2E_BLOB_CIPHERTEXT_LEN_OFFSET, &htons_len, E2E_CIPHERTEXT_LEN_LEN);
}

bool gen_encrypted_framed_message(const uint8_t *plaintext, uint16_t plaintext_len, 
    const uint8_t key[E2E_KEY_LEN], uint8_t *out_frame, uint16_t *out_frame_len)
{
    bool retval = true;
    if(plaintext != NULL && out_frame != NULL && out_frame_len != NULL)
    {
        uint8_t nonce[E2E_NONCE_LEN];
        uint8_t tag[E2E_TAG_LEN];
        uint16_t ciphertext_len;
        uint16_t total_len;
        uint8_t *cipher_txt_ptr = NULL;
        uint8_t *tag_ptr = NULL;
        if(RAND_bytes(nonce,E2E_NONCE_LEN) != TOR_OPENSSL_OK)
        {
            tor_crypto_print_openssl_error("RAND_bytes");
            retval = false;
        }
        else
        {
            cipher_txt_ptr = out_frame + E2E_BLOB_CIPHERTEXT_OFFSET;
            if(!(e2e_encrypt(key,nonce,plaintext,plaintext_len,cipher_txt_ptr,&ciphertext_len,tag)))
            {
                printf("e2e_encrypt func failed\n");
                retval = false;
            }
            else
            {
                total_len = frame_total_len(ciphertext_len);
                memcpy(out_frame + E2E_BLOB_NONCE_OFFSET, nonce, E2E_NONCE_LEN);
                write_frame_len(out_frame,ciphertext_len);
                tag_ptr = out_frame + (E2E_BLOB_CIPHERTEXT_OFFSET + ciphertext_len);
                memcpy(tag_ptr, tag, E2E_TAG_LEN);
                *out_frame_len = total_len;
            }
        }
    }
    else
    {
        retval = false;
    }
    return retval;
}