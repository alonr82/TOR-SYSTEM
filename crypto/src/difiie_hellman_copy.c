#define _POSIX_C_SOURCE 200112L

#include "tor_crypto.h"

#include <string.h>
#include <stdio.h>

#include <openssl/evp.h>
#include <openssl/err.h>

#define TOR_OPENSSL_OK                      1
#define TOR_OPENSSL_FAIL                    0

static void tor_crypto_print_openssl_error(const char* where)
{
    unsigned long err_code = 0;
    char error_text[256];

    err_code = ERR_get_error();
    if (err_code == 0)
    {
        fprintf(stderr, "[TOR_ECDH] %s: OpenSSL error (unknown)\n", where);
    }
    else
    {
        ERR_error_string_n(err_code, error_text, sizeof(error_text));
        fprintf(stderr, "[TOR_ECDH] %s: %s\n", where, error_text);
    }
}

static int tor_validate_ptr(const void* p)
{
    int retval = 0;

    if (p != NULL)
    {
        retval = 1;
    }

    return retval;
}

static int tor_validate_buf_and_len(const void* p, size_t len)
{
    int retval = 0;

    if (p != NULL && len > 0)
    {
        retval = 1;
    }

    return retval;
}

static int tor_x25519_keygen_evp(EVP_PKEY** out_keypair)
{
    int retval = 0;

    EVP_PKEY_CTX* ctx = NULL;
    EVP_PKEY* keypair = NULL;

    if (out_keypair != NULL)
    {
        retval = 1;

        /* יצירת קונטקסט לג׳נרוט מפתחות מסוג X25519 */
        ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_X25519, NULL);
        if (ctx == NULL)
        {
            tor_crypto_print_openssl_error("EVP_PKEY_CTX_new_id(X25519)");
            retval = 0;
        }

        if (retval == 1)
        {
            /* אתחול keygen */
            if (EVP_PKEY_keygen_init(ctx) != TOR_OPENSSL_OK)
            {
                tor_crypto_print_openssl_error("EVP_PKEY_keygen_init");
                retval = 0;
            }
        }

        if (retval == 1)
        {
            /* יצירת keypair בפועל */
            if (EVP_PKEY_keygen(ctx, &keypair) != TOR_OPENSSL_OK)
            {
                tor_crypto_print_openssl_error("EVP_PKEY_keygen");
                retval = 0;
            }
        }

        if (retval == 1)
        {
            *out_keypair = keypair;
            keypair = NULL; /* מעבירים בעלות החוצה */
        }
    }

    if (keypair != NULL)
    {
        EVP_PKEY_free(keypair);
    }

    if (ctx != NULL)
    {
        EVP_PKEY_CTX_free(ctx);
    }

    return retval;
}

static int tor_x25519_export_raw_public(
    EVP_PKEY* keypair,
    uint8_t out_public_key[TOR_X25519_KEY_LEN]
)
{
    int retval = 0;

    size_t pub_len = 0;

    if (keypair != NULL && out_public_key != NULL)
    {
        retval = 1;

        pub_len = TOR_X25519_KEY_LEN;

        /* חילוץ public key גולמי (32B) */
        if (EVP_PKEY_get_raw_public_key(keypair, out_public_key, &pub_len) != TOR_OPENSSL_OK)
        {
            tor_crypto_print_openssl_error("EVP_PKEY_get_raw_public_key");
            retval = 0;
        }

        if (retval == 1)
        {
            if (pub_len != TOR_X25519_KEY_LEN)
            {
                retval = 0;
            }
        }
    }

    return retval;
}

static int tor_x25519_export_raw_private(
    EVP_PKEY* keypair,
    uint8_t out_private_key[TOR_X25519_KEY_LEN]
)
{
    int retval = 0;

    size_t priv_len = 0;

    if (keypair != NULL && out_private_key != NULL)
    {
        retval = 1;

        priv_len = TOR_X25519_KEY_LEN;

        /* חילוץ private key גולמי (32B) */
        if (EVP_PKEY_get_raw_private_key(keypair, out_private_key, &priv_len) != TOR_OPENSSL_OK)
        {
            tor_crypto_print_openssl_error("EVP_PKEY_get_raw_private_key");
            retval = 0;
        }

        if (retval == 1)
        {
            if (priv_len != TOR_X25519_KEY_LEN)
            {
                retval = 0;
            }
        }
    }

    return retval;
}

int tor_x25519_generate_keypair(
    uint8_t out_public_key[TOR_X25519_KEY_LEN],
    uint8_t out_private_key[TOR_X25519_KEY_LEN]
)
{
    int retval = 0;

    EVP_PKEY* keypair = NULL;

    if (out_public_key != NULL && out_private_key != NULL)
    {
        retval = 1;

        /* יצירת keypair פנימי של OpenSSL */
        if (tor_x25519_keygen_evp(&keypair) != 1)
        {
            retval = 0;
        }

        if (retval == 1)
        {
            /* חילוץ public */
            if (tor_x25519_export_raw_public(keypair, out_public_key) != 1)
            {
                retval = 0;
            }
        }

        if (retval == 1)
        {
            /* חילוץ private */
            if (tor_x25519_export_raw_private(keypair, out_private_key) != 1)
            {
                retval = 0;
            }
        }
    }

    if (keypair != NULL)
    {
        EVP_PKEY_free(keypair);
    }

    return retval;
}

static int tor_x25519_import_raw_private_key(
    const uint8_t raw_private_key[TOR_X25519_KEY_LEN],
    EVP_PKEY** out_pkey
)
{
    int retval = 0;

    EVP_PKEY* pkey = NULL;

    if (raw_private_key != NULL && out_pkey != NULL)
    {
        retval = 1;

        /* יצירת EVP_PKEY מתוך raw private (X25519) */
        pkey = EVP_PKEY_new_raw_private_key(EVP_PKEY_X25519, NULL, raw_private_key, TOR_X25519_KEY_LEN);
        if (pkey == NULL)
        {
            tor_crypto_print_openssl_error("EVP_PKEY_new_raw_private_key(X25519)");
            retval = 0;
        }

        if (retval == 1)
        {
            *out_pkey = pkey;
            pkey = NULL; /* מעבירים בעלות החוצה */
        }
    }

    if (pkey != NULL)
    {
        EVP_PKEY_free(pkey);
    }

    return retval;
}

static int tor_x25519_import_raw_public_key(
    const uint8_t raw_public_key[TOR_X25519_KEY_LEN],
    EVP_PKEY** out_pkey
)
{
    int retval = 0;

    EVP_PKEY* pkey = NULL;

    if (raw_public_key != NULL && out_pkey != NULL)
    {
        retval = 1;

        /* יצירת EVP_PKEY מתוך raw public (X25519) */
        pkey = EVP_PKEY_new_raw_public_key(EVP_PKEY_X25519, NULL, raw_public_key, TOR_X25519_KEY_LEN);
        if (pkey == NULL)
        {
            tor_crypto_print_openssl_error("EVP_PKEY_new_raw_public_key(X25519)");
            retval = 0;
        }

        if (retval == 1)
        {
            *out_pkey = pkey;
            pkey = NULL; /* מעבירים בעלות החוצה */
        }
    }

    if (pkey != NULL)
    {
        EVP_PKEY_free(pkey);
    }

    return retval;
}

static int tor_x25519_derive_secret_evp(
    uint8_t out_shared_secret[TOR_SHARED_SECRET_LEN],
    EVP_PKEY* my_private_pkey,
    EVP_PKEY* peer_public_pkey
)
{
    int retval = 0;

    EVP_PKEY_CTX* ctx = NULL;

    size_t secret_len = 0;

    if (out_shared_secret != NULL && my_private_pkey != NULL && peer_public_pkey != NULL)
    {
        retval = 1;

        /* יצירת context ל-derive */
        ctx = EVP_PKEY_CTX_new(my_private_pkey, NULL);
        if (ctx == NULL)
        {
            tor_crypto_print_openssl_error("EVP_PKEY_CTX_new(derive)");
            retval = 0;
        }

        if (retval == 1)
        {
            /* אתחול derive */
            if (EVP_PKEY_derive_init(ctx) != TOR_OPENSSL_OK)
            {
                tor_crypto_print_openssl_error("EVP_PKEY_derive_init");
                retval = 0;
            }
        }

        if (retval == 1)
        {
            /* הגדרת peer (ה-public של הצד השני) */
            if (EVP_PKEY_derive_set_peer(ctx, peer_public_pkey) != TOR_OPENSSL_OK)
            {
                tor_crypto_print_openssl_error("EVP_PKEY_derive_set_peer");
                retval = 0;
            }
        }

        if (retval == 1)
        {
            /* בקשת גודל secret */
            secret_len = 0;
            if (EVP_PKEY_derive(ctx, NULL, &secret_len) != TOR_OPENSSL_OK)
            {
                tor_crypto_print_openssl_error("EVP_PKEY_derive(get_len)");
                retval = 0;
            }
        }

        if (retval == 1)
        {
            /* ב-X25519 חייב להיות 32 */
            if (secret_len != TOR_SHARED_SECRET_LEN)
            {
                retval = 0;
            }
        }

        if (retval == 1)
        {
            /* הפקת shared_secret */
            if (EVP_PKEY_derive(ctx, out_shared_secret, &secret_len) != TOR_OPENSSL_OK)
            {
                tor_crypto_print_openssl_error("EVP_PKEY_derive");
                retval = 0;
            }
        }

        if (retval == 1)
        {
            if (secret_len != TOR_SHARED_SECRET_LEN)
            {
                retval = 0;
            }
        }
    }

    if (ctx != NULL)
    {
        EVP_PKEY_CTX_free(ctx);
    }

    return retval;
}

int tor_x25519_derive_shared_secret(
    uint8_t out_shared_secret[TOR_SHARED_SECRET_LEN],
    const uint8_t my_private_key[TOR_X25519_KEY_LEN],
    const uint8_t peer_public_key[TOR_X25519_KEY_LEN]
)
{
    int retval = 0;

    EVP_PKEY* my_priv = NULL;
    EVP_PKEY* peer_pub = NULL;

    if (out_shared_secret != NULL && my_private_key != NULL && peer_public_key != NULL)
    {
        retval = 1;

        /* import private */
        if (tor_x25519_import_raw_private_key(my_private_key, &my_priv) != 1)
        {
            retval = 0;
        }

        /* import public */
        if (retval == 1)
        {
            if (tor_x25519_import_raw_public_key(peer_public_key, &peer_pub) != 1)
            {
                retval = 0;
            }
        }

        /* derive secret */
        if (retval == 1)
        {
            if (tor_x25519_derive_secret_evp(out_shared_secret, my_priv, peer_pub) != 1)
            {
                retval = 0;
            }
        }
    }

    if (peer_pub != NULL)
    {
        EVP_PKEY_free(peer_pub);
    }

    if (my_priv != NULL)
    {
        EVP_PKEY_free(my_priv);
    }

    return retval;
}

int tor_hkdf_sha256_derive_key(
    uint8_t* out_key, size_t out_key_len,
    const uint8_t* ikm, size_t ikm_len,
    const uint8_t* salt, size_t salt_len,
    const uint8_t* info, size_t info_len
)
{
    int retval = 0;

    EVP_PKEY_CTX* ctx = NULL;

    if (tor_validate_buf_and_len(out_key, out_key_len) == 1 &&
        tor_validate_buf_and_len(ikm, ikm_len) == 1)
    {
        retval = 1;

        /* יצירת context ל-HKDF */
        ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_HKDF, NULL);
        if (ctx == NULL)
        {
            tor_crypto_print_openssl_error("EVP_PKEY_CTX_new_id(HKDF)");
            retval = 0;
        }

        if (retval == 1)
        {
            /* אתחול derive של HKDF */
            if (EVP_PKEY_derive_init(ctx) != TOR_OPENSSL_OK)
            {
                tor_crypto_print_openssl_error("EVP_PKEY_derive_init(HKDF)");
                retval = 0;
            }
        }

        if (retval == 1)
        {
            /* בחירת Hash ל-HKDF: SHA256 */
            if (EVP_PKEY_CTX_set_hkdf_md(ctx, EVP_sha256()) != TOR_OPENSSL_OK)
            {
                tor_crypto_print_openssl_error("EVP_PKEY_CTX_set_hkdf_md(SHA256)");
                retval = 0;
            }
        }

        /* salt הוא אופציונלי */
        if (retval == 1)
        {
            if (salt != NULL && salt_len > 0)
            {
                if (EVP_PKEY_CTX_set1_hkdf_salt(ctx, salt, (int)salt_len) != TOR_OPENSSL_OK)
                {
                    tor_crypto_print_openssl_error("EVP_PKEY_CTX_set1_hkdf_salt");
                    retval = 0;
                }
            }
        }

        if (retval == 1)
        {
            /* IKM = shared_secret (או חומר מפתח אחר) */
            if (EVP_PKEY_CTX_set1_hkdf_key(ctx, ikm, (int)ikm_len) != TOR_OPENSSL_OK)
            {
                tor_crypto_print_openssl_error("EVP_PKEY_CTX_set1_hkdf_key");
                retval = 0;
            }
        }

        /* info הוא אופציונלי */
        if (retval == 1)
        {
            if (info != NULL && info_len > 0)
            {
                if (EVP_PKEY_CTX_add1_hkdf_info(ctx, info, (int)info_len) != TOR_OPENSSL_OK)
                {
                    tor_crypto_print_openssl_error("EVP_PKEY_CTX_add1_hkdf_info");
                    retval = 0;
                }
            }
        }

        if (retval == 1)
        {
            /* הפקת המפתח הסופי */
            if (EVP_PKEY_derive(ctx, out_key, &out_key_len) != TOR_OPENSSL_OK)
            {
                tor_crypto_print_openssl_error("EVP_PKEY_derive(HKDF)");
                retval = 0;
            }
        }
    }

    if (ctx != NULL)
    {
        EVP_PKEY_CTX_free(ctx);
    }

    return retval;
}

int tor_derive_e2e_key_from_shared_secret(
    uint8_t out_e2e_key[TOR_E2E_KEY_LEN],
    const uint8_t shared_secret[TOR_SHARED_SECRET_LEN],
    const char* info_label_string
)
{
    int retval = 0;

    uint8_t info_buf[TOR_HKDF_INFO_MAX_LEN];
    size_t info_len = 0;

    if (out_e2e_key != NULL && shared_secret != NULL && info_label_string != NULL)
    {
        retval = 1;

        /* הכנת info (label) ל-HKDF כדי להבדיל מפתחות שונים */
        memset(info_buf, 0, sizeof(info_buf));
        info_len = strnlen(info_label_string, TOR_HKDF_INFO_MAX_LEN);

        if (info_len == 0 || info_len >= TOR_HKDF_INFO_MAX_LEN)
        {
            retval = 0;
        }

        if (retval == 1)
        {
            memcpy(info_buf, info_label_string, info_len);

            /* salt לא חובה בשלב ראשון -> מעבירים NULL/0 */
            if (tor_hkdf_sha256_derive_key(
                    out_e2e_key,
                    TOR_E2E_KEY_LEN,
                    shared_secret,
                    TOR_SHARED_SECRET_LEN,
                    NULL,
                    0,
                    info_buf,
                    info_len
                ) != 1)
            {
                retval = 0;
            }
        }
    }

    return retval;
}
