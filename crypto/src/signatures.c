#define _POSIX_C_SOURCE 200112L
#include "tor_crypto.h"


static bool get_raw_ed25519_private_seed(EVP_PKEY *private_key, uint8_t seed_out[TOR_ED25519_SEED_LEN])
{
    bool retval = true;
    size_t private_key_len = 0;
    if (EVP_PKEY_get_raw_private_key(private_key, NULL, &private_key_len) <= 0)
    {
        retval = false;
    }
    else
    {
        uint8_t tmp[TOR_ED25519_SIG_LEN];
        if (private_key_len > sizeof(tmp))
        {
            retval = false;
        }
        else
        {
            if (EVP_PKEY_get_raw_private_key(private_key, tmp, &private_key_len) <= 0)
            {
                retval = false;
            }
            else
            {
                memcpy(seed_out, tmp, TOR_ED25519_SEED_LEN);
            }
        }
    }
    return retval;
}

static bool gen_public_key(EVP_PKEY_CTX *context,uint8_t pub[TOR_ED25519_PUB_LEN],
    EVP_PKEY **pkey_out)
{
    bool retval = true;
    if (EVP_PKEY_keygen_init(context) <= 0)
    {
        retval = false;
    }
    else
    {
        EVP_PKEY *pkey = NULL;
        if (EVP_PKEY_keygen(context, &pkey) <= 0)
        {
            retval = false;
        }
        else
        {
            size_t publen = TOR_ED25519_PUB_LEN;

            if (EVP_PKEY_get_raw_public_key(pkey, pub, &publen) <= 0 || publen != TOR_ED25519_PUB_LEN)
            {
                EVP_PKEY_free(pkey);
                retval = false;
            }
            else
            {
                *pkey_out = pkey; 
            }
        }
    }

    return retval;
}

bool tor_ed25519_generate_identity_keypair(uint8_t pub[TOR_ED25519_PUB_LEN], uint8_t priv_seed[TOR_ED25519_SEED_LEN])
{
    bool retval = true;
    EVP_PKEY_CTX *context = EVP_PKEY_CTX_new_id(EVP_PKEY_ED25519, NULL);
    if (!context)
    {
        return false;
    }
    else
    {
        EVP_PKEY *pkey = NULL;
        if(!gen_public_key(context, pub, &pkey))
        {
            retval = false;
        }
        else
        {
            if (!get_raw_ed25519_private_seed(pkey, priv_seed))
            {

                EVP_PKEY_free(pkey);
                EVP_PKEY_CTX_free(context);
                retval = false;
            }
        }
        if(pkey != NULL)
        {
            EVP_PKEY_free(pkey);
        }
    }
    EVP_PKEY_CTX_free(context);
    return retval;
}

static bool sign_message(EVP_PKEY *pkey, uint8_t sig[TOR_ED25519_SIG_LEN], 
    const uint8_t *msg, size_t *msg_len, EVP_MD_CTX *mctx)
{
    bool retval = true;
    if (!mctx)
    {
        EVP_PKEY_free(pkey);
        retval = false;
    }
    else
    {
        if (EVP_DigestSignInit(mctx, NULL, NULL, NULL, pkey) <= 0)
        {
            EVP_MD_CTX_free(mctx);
            EVP_PKEY_free(pkey);
            retval = false;
        }
        size_t siglen = TOR_ED25519_SIG_LEN;
        if (EVP_DigestSign(mctx, sig, &siglen, msg, *msg_len) <= 0 || siglen != TOR_ED25519_SIG_LEN)
        {
            EVP_MD_CTX_free(mctx);
            EVP_PKEY_free(pkey);
            retval = false;
        }
    }
    return retval;
}

bool tor_ed25519_sign(uint8_t sig[TOR_ED25519_SIG_LEN], const uint8_t priv_seed[TOR_ED25519_SEED_LEN],
    const uint8_t *msg, size_t msg_len)
{
    bool retval = true;
    EVP_PKEY *pkey = EVP_PKEY_new_raw_private_key(EVP_PKEY_ED25519, NULL, priv_seed, TOR_ED25519_SEED_LEN);
    if (!pkey)
    {
        return false;
    }
    else
    {
        EVP_MD_CTX *mctx = EVP_MD_CTX_new();
        if(!sign_message(pkey, sig, msg, &msg_len, mctx))
        {
            retval = false;
        }
        if(mctx != NULL)
        {
            EVP_MD_CTX_free(mctx);
        }
    }
    EVP_PKEY_free(pkey);
    return retval;
}

static bool verify_message(EVP_MD_CTX *mctx, EVP_PKEY *pkey, const uint8_t sig[TOR_ED25519_SIG_LEN], 
    const uint8_t *msg, size_t *msg_len)
{
    bool retval = true;
    if (!mctx)
    {
        EVP_PKEY_free(pkey);
        retval = false;
    }
    else
    {
        if (EVP_DigestVerifyInit(mctx, NULL, NULL, NULL, pkey) <= 0)
        {
            retval = false;
        }
        else
        {
            int verify_res = EVP_DigestVerify(mctx, sig, TOR_ED25519_SIG_LEN, msg, *msg_len);
            retval = (verify_res == true);
        }
    }
    return retval;
}

bool tor_ed25519_verify(const uint8_t pub[TOR_ED25519_PUB_LEN],
    const uint8_t sig[TOR_ED25519_SIG_LEN], const uint8_t *msg, size_t msg_len)
{
    bool retval = true;
    EVP_PKEY *pkey = EVP_PKEY_new_raw_public_key(EVP_PKEY_ED25519, NULL, pub, TOR_ED25519_PUB_LEN);
    if (!pkey)
    {
        retval = false;
    }
    else
    {
        EVP_MD_CTX *mctx = EVP_MD_CTX_new();
        retval = verify_message(mctx,pkey,sig,msg,&msg_len);
        EVP_MD_CTX_free(mctx);
    }
    if(pkey != NULL)
    {
        EVP_PKEY_free(pkey);
    }
    return retval;
}
