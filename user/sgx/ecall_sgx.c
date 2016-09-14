#include "enclave_private.h"
#include "seqptrmap.h"

#include "../types.h"

#include <string.h>

#include <sgx_tseal.h>
#include <sgx_utils.h>

#include <mbedtls/aes.h>
#include <mbedtls/md.h>
#include <mbedtls/sha256.h>

sgx_key_128bit_t __TOPSECRET__ __enclave_encryption_key__;

typedef struct {
    crypto_context_t crypto_data;
    crypto_iv_t iv;
    mbedtls_aes_context aes_ctx;
    mbedtls_md_context_t hmac_ctx;
    uint8_t * p_input;
} crypto_ctx_t;

struct seqptrmap * crypto_hashmap = NULL;

int ecall_init_enclave()
{
    sgx_key_request_t request;
    sgx_status_t status;
    int ret;

    memset(&request, 0, sizeof(sgx_key_request_t));
    request.key_name = SGX_KEYSELECT_SEAL;
    request.key_policy = SGX_KEYPOLICY_MRSIGNER;
    request.attribute_mask.flags = 0xfffffffffffffff3ULL;
    request.attribute_mask.xfrm = 0;

    status = sgx_get_key(&request, &__enclave_encryption_key__);
    if (status != SGX_SUCCESS) {
        ret = E_ERROR_KEYINIT;
        goto out;
    }

    if ((crypto_hashmap = seqptrmap_init()) == NULL) {
        ret = E_ERROR_HASHMAP;
        goto out;
    }

    ret = E_SUCCESS;
out:
    return ret;
}

/**
 * Starts the file crypto
 * @return 0 on success
 */
int ecall_init_crypto(xfer_context_t * xfer_ctx, crypto_context_t * fcrypto)
{
    int error = E_ERROR_CRYPTO;
    crypto_ctx_t __SECRET * __ctx;
    mbedtls_md_context_t * hmac_ctx;

    if ((__ctx = (crypto_ctx_t *)calloc(1, sizeof(crypto_ctx_t))) == NULL) {
        return E_ERROR_CRYPTO;
    }

    /* copy the crypto data */
    memcpy(&__ctx->crypto_data, fcrypto, sizeof(crypto_context_t));
    if (xfer_ctx->op == UCPRIV_ENCRYPT) {
        sgx_read_rand((uint8_t *)&__ctx->iv, sizeof(crypto_iv_t));
        memcpy(&__ctx->crypto_data.iv, &__ctx->iv, sizeof(crypto_iv_t));
    } else {
        memcpy(&__ctx->iv, &__ctx->crypto_data.iv, sizeof(crypto_iv_t));
    }

    /* initialize the crypto context*/
    hmac_ctx = &__ctx->hmac_ctx;
    mbedtls_md_init(hmac_ctx);
    if (mbedtls_md_setup(hmac_ctx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256),
                         1)) {
        goto out;
    }

    if (mbedtls_md_hmac_starts(hmac_ctx, (uint8_t *)&__ctx->crypto_data.skey,
                               sizeof(crypto_ekey_t))) {
        goto out;
    }

    mbedtls_aes_init(&__ctx->aes_ctx);
    if (xfer_ctx->op == UCPRIV_ENCRYPT) {
        if (mbedtls_aes_setkey_enc(&__ctx->aes_ctx,
                                   (uint8_t *)&__ctx->crypto_data.ekey,
                                   CRYPTO_AES_KEY_SIZE_BITS)) {
            goto out;
        }
    } else {
        if (mbedtls_aes_setkey_dec(&__ctx->aes_ctx,
                                   (uint8_t *)&__ctx->crypto_data.ekey,
                                   CRYPTO_AES_KEY_SIZE_BITS)) {
            goto out;
        }
    }

    if ((xfer_ctx->crypto_id = seqptrmap_add(crypto_hashmap, __ctx)) == -1) {
        error = E_ERROR_ERROR;
        goto out;
    }

    __ctx->p_input = (uint8_t *)calloc(1, E_CRYPTO_BUFFER_LEN);
    if (__ctx->p_input == NULL) {
        error = E_ERROR_ALLOC;
        goto out;
    }

    // we only pad on decryption
    xfer_ctx->padded_len = xfer_ctx->op == UCPRIV_ENCRYPT
                               ? CRYPTO_CEIL_TO_BLKSIZE(xfer_ctx->raw_len)
                               : xfer_ctx->raw_len;
    error = E_SUCCESS;
out:
    if (error) {
        free(__ctx);
    }
    return error;
}

int ecall_crypt_data(xfer_context_t * xfer_ctx)
{
    int error = E_ERROR_ERROR;
    size_t nbytes, i, padded_len, total_len;
    mbedtls_aes_context * aes_ctx;
    mbedtls_md_context_t * hmac_ctx;
    uint8_t * p_input, *p_output, *iv;
    int pad, pkcs, bytes_left;
    crypto_ctx_t __SECRET * __ctx;

    if ((__ctx = seqptrmap_get(crypto_hashmap, xfer_ctx->crypto_id)) == NULL) {
        goto out;
    }

    if (xfer_ctx->padded_len % CRYPTO_CRYPTO_BLK_SIZE) {
        goto out;
    }

    p_input = __ctx->p_input;
    iv = (uint8_t *)&__ctx->iv;

    bytes_left = xfer_ctx->valid_buflen;
    padded_len = xfer_ctx->padded_len;
    total_len = xfer_ctx->raw_len;

    aes_ctx = &__ctx->aes_ctx;
    hmac_ctx = &__ctx->hmac_ctx;

    pad = (bytes_left + xfer_ctx->completed >= xfer_ctx->raw_len);
    p_output = xfer_ctx->buffer;

    while (bytes_left > 0) {
        nbytes = bytes_left > E_CRYPTO_BUFFER_LEN ? E_CRYPTO_BUFFER_LEN
                                                  : bytes_left;
        memcpy(p_input, p_output, nbytes);

        if (xfer_ctx->op == UCPRIV_ENCRYPT) {
            if (bytes_left <= E_CRYPTO_BUFFER_LEN && pad) {
                pkcs = padded_len - xfer_ctx->raw_len;
                // set the padding
                memset(p_input + nbytes - pkcs, pkcs, pkcs);
            }
            mbedtls_aes_crypt_cbc(aes_ctx, MBEDTLS_AES_ENCRYPT, nbytes, iv,
                                  p_input, p_input);
        }

        mbedtls_md_hmac_update(hmac_ctx, p_input, nbytes);

        if (xfer_ctx->op == UCPRIV_DECRYPT) {
            mbedtls_aes_crypt_cbc(aes_ctx, MBEDTLS_AES_DECRYPT, nbytes, iv,
                                  p_input, p_input);

            /* if we're in the last loop, we can adjust the padding */
            if (bytes_left <= E_CRYPTO_BUFFER_LEN && pad) {
                xfer_ctx->raw_len -= (int)(p_input[nbytes - 1]);
            }
        }

        memcpy(p_output, p_input, nbytes);
        bytes_left -= nbytes;
        p_output += nbytes;
    }

    // TODO clear the p_input buffer

    error = E_SUCCESS;
out:
    return error;
}

int ecall_finish_crypto(xfer_context_t * xfer_ctx, crypto_context_t * fcrypto)
{
    int error = E_ERROR_ERROR;
    crypto_mac_t mac;
    crypto_ctx_t __SECRET * __ctx;

    if ((__ctx = seqptrmap_get(crypto_hashmap, xfer_ctx->crypto_id)) == NULL) {
        goto out;
    }

    mbedtls_aes_free(&__ctx->aes_ctx);

    /* close the crypto context and verify the mac */
    mbedtls_md_hmac_finish(&__ctx->hmac_ctx, (uint8_t *)&mac);
    if (xfer_ctx->op == UCPRIV_DECRYPT
        && memcmp(&mac, &__ctx->crypto_data.mac, sizeof(crypto_mac_t))) {
        goto out;
    } else {
        memcpy(&__ctx->crypto_data.mac, &mac, sizeof(crypto_mac_t));
    }

    mbedtls_md_free(&__ctx->hmac_ctx);

    // TODO seal the keys before sending

    memcpy(fcrypto, &__ctx->crypto_data, sizeof(crypto_context_t));
    memset(__ctx, 0, sizeof(crypto_ctx_t));
    error = E_SUCCESS;
out:
    free(__ctx->p_input);
    free(__ctx);
    seqptrmap_delete(crypto_hashmap, xfer_ctx->crypto_id);
    return error;
}