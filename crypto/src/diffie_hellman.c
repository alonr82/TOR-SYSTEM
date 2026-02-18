#include "tor_crypto.h"
#include <string.h>
#include <stdio.h>
#include <openssl/evp.h>
#include <openssl/err.h>

static void free_context(EVP_PKEY_CTX *context)
{
    if(context != NULL)
    {
        EVP_PKEY_CTX_free(context);
    }
}

static bool generate_empherial_keys(EVP_PKEY **keypair_out)
{
    bool retval = true;
    EVP_PKEY_CTX* context = NULL;
    EVP_PKEY* keypair = NULL;
    if (keypair_out != NULL)
    {
        context = EVP_PKEY_CTX_new_id(EVP_PKEY_X25519, NULL);
        if (context == NULL)
        {
            tor_crypto_print_openssl_error("EVP_PKEY_CTX_new_id(X25519)");
            retval = false;
        }
        else
        {
            if (EVP_PKEY_keygen_init(context) != TOR_OPENSSL_OK)
            {
                tor_crypto_print_openssl_error("EVP_PKEY_keygen_init");
                retval = false;
            }
            else
            {
                if (EVP_PKEY_keygen(context, &keypair) != TOR_OPENSSL_OK)
                {
                    tor_crypto_print_openssl_error("EVP_PKEY_keygen");
                    retval = 0;
                }
                else
                {
                    *keypair_out = keypair;
                    keypair = NULL;
                }
            }
        }
        
    }

    if (keypair != NULL)
    {
        EVP_PKEY_free(keypair);
    }

    free_context(context);

    return retval;
}

static bool export_empherial_raw_public(EVP_PKEY* keypair,
    uint8_t out_public_key[TOR_X25519_KEY_LEN])
{
    bool retval = true;
    size_t pub_len = 0;
    if (keypair != NULL && out_public_key != NULL)
    {
        pub_len = TOR_X25519_KEY_LEN;
        if (EVP_PKEY_get_raw_public_key(keypair, out_public_key, &pub_len) != TOR_OPENSSL_OK)
        {
            tor_crypto_print_openssl_error("EVP_PKEY_get_raw_public_key");
            retval = 0;
        }
        else
        {
            if(pub_len != TOR_X25519_KEY_LEN)
            {
                printf("public key size is wrong\n");
                retval = false;
            }
        }
    }

    return retval;
}

static bool import_empherial_raw_private(const uint8_t raw_private_key[TOR_X25519_KEY_LEN],
    EVP_PKEY **out_key)
{
    bool retval = true;
    EVP_PKEY *pkey = NULL;

    if(raw_private_key != NULL && out_key != NULL)
    {
        pkey = EVP_PKEY_new_raw_private_key(EVP_PKEY_X25519,NULL, raw_private_key, TOR_X25519_KEY_LEN);
        if(pkey == NULL)
        {
            tor_crypto_print_openssl_error("EVP_PKEY_new_raw_private_key");
            retval = false;
        }
        else
        {
            *out_key = pkey;
            pkey = NULL;
        }
    }
    if(pkey != NULL)
    {
        EVP_PKEY_free(pkey);
    }
    return retval;
}

static bool import_empherial_raw_public(const uint8_t raw_public_key[TOR_X25519_KEY_LEN],
    EVP_PKEY **out_key)
{
    bool retval = true;
    EVP_PKEY *pkey = NULL;

    if(raw_public_key != NULL && out_key != NULL)
    {
        pkey = EVP_PKEY_new_raw_public_key(EVP_PKEY_X25519,NULL, raw_public_key, TOR_X25519_KEY_LEN);
        if(pkey == NULL)
        {
            tor_crypto_print_openssl_error("EVP_PKEY_new_raw_private_key");
            retval = false;
        }
        else
        {
            *out_key = pkey;
            pkey = NULL;
        }
    }
    if(pkey != NULL)
    {
        EVP_PKEY_free(pkey);
    }
    return retval;
}

static bool export_empherial_raw_private(EVP_PKEY* keypair,
    uint8_t out_private_key[TOR_X25519_KEY_LEN])
{
    bool retval = true;
    size_t private_len = 0;
    if (keypair != NULL && out_private_key != NULL)
    {
        private_len = TOR_X25519_KEY_LEN;
        if (EVP_PKEY_get_raw_private_key(keypair, out_private_key, &private_len) != TOR_OPENSSL_OK)
        {
            tor_crypto_print_openssl_error("EVP_PKEY_get_raw_public_key");
            retval = 0;
        }
        else
        {
            if(private_len != TOR_X25519_KEY_LEN)
            {
                printf("public key size is wrong\n");
                retval = false;
            }
        }
    }

    return retval;
}

bool generate_empherial_keypair(uint8_t empherial_public_key[TOR_X25519_KEY_LEN],
    uint8_t empherial_private_key[TOR_X25519_KEY_LEN])
{
    bool retval = true;
    EVP_PKEY *keypair = NULL;

    if(empherial_private_key != NULL && empherial_public_key != NULL)
    {
        if(!generate_empherial_keys(&keypair))
        {
            printf("failed function: generate_empherial_keys\n");
            retval = false;
        }
        else
        {
            if(export_empherial_raw_public(keypair,empherial_public_key) &&
                export_empherial_raw_private(keypair,empherial_private_key))
            {
                printf("generated keys succefully\n");
            }
            else
            {
                printf("failed to generate at least one of the keys\n");
                retval = false;
            }
        }
    }
    if(keypair != NULL)
    {
        EVP_PKEY_free(keypair);
    }
    return retval;
}

static bool init_context_derive_share_secret(EVP_PKEY_CTX *context, EVP_PKEY *self_private_key,
    EVP_PKEY *public_key_partner, size_t *shared_secret_len)
{
    bool retval = true;
    context = EVP_PKEY_CTX_new(self_private_key, NULL);
    if(context == NULL)
    {
        tor_crypto_print_openssl_error("EVP_PKEY_CTX_new");
        retval = false;
    }
    else
    {
        if(EVP_PKEY_derive_init(context) != TOR_OPENSSL_OK)
        {
            tor_crypto_print_openssl_error("EVP_PKEY_derive_init");
            retval = false;
        }
        else
        {
            if(EVP_PKEY_derive_set_peer(context, public_key_partner) != TOR_OPENSSL_OK)
            {
                tor_crypto_print_openssl_error("EVP_PKEY_derive_set_peer");
                retval = false;
            }
            else
            {
                if(EVP_PKEY_derive(context, NULL, shared_secret_len) != TOR_OPENSSL_OK)
                {
                    tor_crypto_print_openssl_error("EVP_PKEY_derive");
                    retval = false;
                }
                else
                {
                    if(*shared_secret_len != TOR_SHARED_SECRET_LEN)
                    {
                        retval = false;
                    }
                }
            }
        }
    }
    return retval;
}

static bool derive_secret(uint8_t shared_secret[TOR_SHARED_SECRET_LEN],
    EVP_PKEY *self_private_key, EVP_PKEY *public_key_partner)
{
    bool retval = true;
    EVP_PKEY_CTX *context = NULL;
    size_t shared_secret_len = 0;
    if(shared_secret != NULL && self_private_key != NULL && public_key_partner != NULL)
    {
        if(!init_context_derive_share_secret(context,self_private_key,public_key_partner,&shared_secret_len))
        {
            retval = false;
        }
        else
        {
            if(EVP_PKEY_derive(context,shared_secret, &shared_secret_len) != TOR_OPENSSL_OK)
            {
                tor_crypto_print_openssl_error("EVP_PKEY_derive");
                retval = false;
            }
            else
            {
                if(shared_secret_len != TOR_SHARED_SECRET_LEN)
                {
                    retval = false;
                }
            }
        }
    }
    else
    {
        retval = false;
    }
    free_context(context);
    return retval;
}

bool derive_shared_secret(uint8_t shared_secret[TOR_SHARED_SECRET_LEN],
    const uint8_t own_private_key[TOR_X25519_KEY_LEN], const uint8_t partner_public_key[TOR_E2E_KEY_LEN])
{
    bool retval = true;
    EVP_PKEY *private_key_self = NULL; 
    EVP_PKEY *public_key_partner = NULL;

    if(shared_secret != NULL && own_private_key != NULL && partner_public_key != NULL)
    {
        if(!import_empherial_raw_private(own_private_key, &private_key_self))
        {
            retval = false;
        }
        else
        {
            if(!import_empherial_raw_public(partner_public_key, &public_key_partner))
            {
                retval = false;
            }
        }
        if(retval)
        {
            if(!derive_secret(shared_secret,private_key_self,public_key_partner))
            {
                retval = false;
            }
            else
            {

            }
        }
    }
    else
    {
        retval = false;
    }
    if(private_key_self != NULL)
    {
        EVP_PKEY_free(private_key_self);
    }
    if(public_key_partner != NULL)
    {
        EVP_PKEY_free(public_key_partner);
    }
    return retval;
}

static bool hkdf_optional_init(const uint8_t *salt, size_t *salt_len,
    const uint8_t *info, size_t *info_len, EVP_PKEY_CTX *context)
{
    bool retval = true;
    if (salt != NULL && *salt_len > 0)
    {
        if (EVP_PKEY_CTX_set1_hkdf_salt(context, salt, (int)*salt_len) != TOR_OPENSSL_OK)
        {
            tor_crypto_print_openssl_error("EVP_PKEY_CTX_set1_hkdf_salt");
            retval = false;;
        }
    }
    else
    {
        if (info != NULL && *info_len > 0)
        {
            if (EVP_PKEY_CTX_add1_hkdf_info(context, info, (int)*info_len) != TOR_OPENSSL_OK)
            {
                tor_crypto_print_openssl_error("EVP_PKEY_CTX_add1_hkdf_info");
                retval = 0;
            }
        }
    }
    return retval;
}

static bool hkdf_derive_process(EVP_PKEY_CTX *context, const uint8_t *salt,
    size_t *salt_len, const uint8_t *info, size_t *info_len, 
    const uint8_t *in_shared_secret, size_t  *in_shared_secret_len, 
    uint8_t *key_derived, size_t *key_derived_len)
{
    bool retval = true;
    if(EVP_PKEY_derive_init(context) != TOR_OPENSSL_OK)
    {
        tor_crypto_print_openssl_error("EVP_PKEY_derive_init");
        retval = false;
    }
    else
    {
        if(EVP_PKEY_CTX_set_hkdf_md(context, EVP_sha256()) != TOR_OPENSSL_OK)
        {
            tor_crypto_print_openssl_error("EVP_PKEY_CTX_set_hkdf_md");
            retval = false;
        }
        else
        {
            if(!hkdf_optional_init(salt, salt_len, info, info_len, context))
            {
                retval = false;
            }
            else
            {
                if (EVP_PKEY_CTX_set1_hkdf_key(context, in_shared_secret, (int)*in_shared_secret_len) != TOR_OPENSSL_OK)
                {
                    tor_crypto_print_openssl_error("EVP_PKEY_CTX_set1_hkdf_key");
                    retval = false;
                }
                else
                {
                    if (EVP_PKEY_derive(context, key_derived, key_derived_len) != TOR_OPENSSL_OK)
                    {
                        tor_crypto_print_openssl_error("EVP_PKEY_derive(HKDF)");
                        retval = false;
                    }
                }
            }
        }
    }
    return retval;
}

bool hkdf_derive_key(uint8_t* key_derived, size_t key_derived_len,
    const uint8_t* in_shared_secret, size_t in_shared_secret_len,
    const uint8_t* salt, size_t salt_len, const uint8_t* info, size_t info_len)
{
    bool retval = true;
    EVP_PKEY_CTX *context = NULL;
    context = EVP_PKEY_CTX_new_id(EVP_PKEY_HKDF, NULL);
    if(context != NULL)
    {
        if(!hkdf_derive_process(context,salt,&salt_len,info,&info_len,
                                in_shared_secret, &in_shared_secret_len, 
                                key_derived, &key_derived_len))
        {
            retval = false;
        }
    }
    else
    {
        tor_crypto_print_openssl_error("EVP_PKEY_CTX_new_id");
        retval = false;
    }
    free_context(context);
    return retval;
}