#include "enclave_internal.h"



#define NEXUS_MAGIC_V1 0x6e780001



/* Crypto buffer serialized format */
/*
   uint32_t magic;   // to determine which algorithm to use
   uint32_t version; // Version of the data
   uint32_t size;    // size of the encrypted buffer

   crypto_ctx;       // Contains the sealed crypto information

   Data
*/

#define GCM128_WRAPPED_KEY_SIZE  (GCM128_KEY_SIZE + GCM128_TAG_SIZE)

struct __gcm_header {
    uint8_t                     ekey[GCM128_WRAPPED_KEY_SIZE];
    uint8_t                     iv[GCM128_IV_SIZE];
    uint8_t                     mac[NEXUS_MAC_SIZE];
} __attribute__((packed));


struct crypto_buf_hdr {
    uint32_t                    magic;

    uint32_t                    version;

    uint32_t                    size; // size of the encrypted buffer

    struct __gcm_header         gcm_hdr;
} __attribute__((packed));






struct nexus_crypto_buf {
    struct nexus_crypto_ctx     crypto_ctx;

    uint32_t                    version;

    size_t                      data_size;

    struct nexus_uuid           uuid;

    uint8_t                   * external_addr;
    size_t                      external_size;

    uint8_t                   * internal_addr;
    size_t                      internal_size;
};




static size_t
__get_header_len()
{
      return sizeof(struct crypto_buf_hdr);
}


struct nexus_crypto_buf *
nexus_crypto_buf_new(size_t size, uint32_t version, struct nexus_uuid * uuid)
{
    struct nexus_crypto_buf * crypto_buf = nexus_malloc(sizeof(struct nexus_crypto_buf));

    crypto_buf->internal_addr = nexus_malloc(size);
    crypto_buf->internal_size = size;

    crypto_buf->version       = version;

    nexus_uuid_copy(uuid, &crypto_buf->uuid);

    return crypto_buf;
}

struct nexus_crypto_buf *
nexus_crypto_buf_create(struct nexus_uuid * buf_uuid, nexus_io_flags_t flags)
{
    struct nexus_crypto_buf * crypto_buf = NULL;

    void   * external_addr = NULL;
    size_t   external_size = 0;


    external_addr = buffer_layer_get(buf_uuid, flags, &external_size);

    if (external_addr == NULL) {
        log_error("Could not retrieve external addr for buffer\n");
        return NULL;
    }

    crypto_buf = nexus_malloc(sizeof(struct nexus_crypto_buf));

    nexus_uuid_copy(buf_uuid, &(crypto_buf->uuid));

    crypto_buf->external_addr = external_addr;
    crypto_buf->external_size = external_size;

    // get the version
    if (external_size >= __get_header_len()) {
        struct crypto_buf_hdr * buf_hdr = (struct crypto_buf_hdr *) external_addr;

        crypto_buf->version = buf_hdr->version;
    } else if (external_size == 0) {
        crypto_buf->version = 0;
    } else {
        crypto_buf->version = CRYPTOBUF_INVALID_VERSION;
    }

    return crypto_buf;
}

void
nexus_crypto_buf_free(struct nexus_crypto_buf * crypto_buf)
{
    assert(crypto_buf != NULL);

    if (crypto_buf->internal_addr) {
        nexus_free(crypto_buf->internal_addr);
    }

    nexus_crypto_ctx_free(&(crypto_buf->crypto_ctx));

    nexus_free(crypto_buf);
}

uint32_t
nexus_crypto_buf_version(struct nexus_crypto_buf * buf)
{
    return buf->version;
}

void
nexus_crypto_buf_set_datasize(struct nexus_crypto_buf * buf, size_t data_size)
{
    buf->data_size = data_size;
}

int
nexus_crypto_buf_sha256_exterior(struct nexus_crypto_buf * crypto_buf,
                                 mbedtls_sha256_context  * sha_context)
{
    if (crypto_buf->external_addr == NULL) {
        log_error("crypto_buf has no external address\n");
        return -1;
    }

    mbedtls_sha256_update(sha_context, crypto_buf->external_addr, crypto_buf->external_size);

    return 0;
}

static int
__parse_gcm128_context(struct nexus_crypto_buf  * crypto_buf,
                       struct crypto_buf_hdr    * buf_hdr)
{
    struct nexus_crypto_ctx * crypto_ctx = &(crypto_buf->crypto_ctx);

    struct nexus_key wrapped_key;


    nexus_init_key(&wrapped_key, NEXUS_WRAPPED_128_KEY);

    if (__nexus_key_from_buf(&wrapped_key,
                               NEXUS_WRAPPED_128_KEY,
                               buf_hdr->gcm_hdr.ekey,
                               GCM128_WRAPPED_KEY_SIZE)) {
        log_error("Error retrieving wrapped key from GCM header\n");
        goto err;
    }


    nexus_key_set_uuid(&wrapped_key, &crypto_buf->uuid);

    if (__nexus_derive_key(&(crypto_ctx->key), NEXUS_RAW_128_KEY, &wrapped_key)) {
        nexus_free_key(&wrapped_key);
        log_error("Error Deriving key from GCM header\n");
        goto err;
    }

    nexus_free_key(&wrapped_key);


    // IV
    if (__nexus_key_from_buf(&(crypto_ctx->iv),
                              NEXUS_RAW_128_KEY,
                              buf_hdr->gcm_hdr.iv,
                              GCM128_IV_SIZE)) {
        log_error("Could not retrieve IV from GCM header\n");
        goto err;
    }

    // MAC
    if (__nexus_mac_from_buf(&(crypto_ctx->mac), buf_hdr->gcm_hdr.mac)) {
        log_error("Could not retrieve MAC from GCM header\n");
        goto err;
    }

    return 0;

 err:

    return -1;
}

static struct crypto_buf_hdr *
__parse_header(struct nexus_crypto_buf * crypto_buf)
{
    struct crypto_buf_hdr * buf_hdr = nexus_malloc(sizeof(struct nexus_crypto_buf));

    memcpy(buf_hdr, crypto_buf->external_addr, __get_header_len());

    if (buf_hdr->magic != NEXUS_MAGIC_V1) {
        log_error("invalid magic value in crypto_buffer\n");
        goto err;
    }

    if (buf_hdr->size != (crypto_buf->external_size - __get_header_len())) {
        log_error("Size mismatch in crypto_buffer\n");
        goto err;
    }


    if (__parse_gcm128_context(crypto_buf, buf_hdr)) {
        log_error("Failed to parse GCM header\n");
        goto err;
    }

    return buf_hdr;

 err:
    nexus_free(buf_hdr);
    return NULL;
}

void *
nexus_crypto_buf_get(struct nexus_crypto_buf * crypto_buf,
                     size_t                  * buffer_size,
                     struct nexus_mac        * mac)
{
    struct crypto_buf_hdr * buf_hdr = NULL;

    /* Internal buffer already exists */
    if (crypto_buf->internal_addr != NULL) {
        *buffer_size = crypto_buf->internal_size;

        return crypto_buf->internal_addr;
    }



    if (crypto_buf->external_addr == NULL || crypto_buf->external_size == 0) {
        log_error("Error: Uninitialized crypto_buf\n");
        return NULL;
    }


    // parses and unwraps the buffer's crypto context
    buf_hdr = __parse_header(crypto_buf);

    if (buf_hdr == NULL) {
        log_error("parsing crypto_buf header FAILED\n");
        goto err;
    }

    /* Allocate internal memory */
    crypto_buf->internal_size = crypto_buf->external_size - __get_header_len();
    crypto_buf->internal_addr = nexus_malloc(crypto_buf->internal_size);

    /* Decrypt the buffer */
    {
        memset(buf_hdr->gcm_hdr.mac, 0, NEXUS_MAC_SIZE);

        if (crypto_gcm_decrypt(&(crypto_buf->crypto_ctx),
                                 crypto_buf->internal_size,
                                 crypto_buf->external_addr + __get_header_len(),
                                 crypto_buf->internal_addr,
                                 mac,
                                 (uint8_t *) (buf_hdr),
                                 __get_header_len())) {
            log_error("crypto_gcm_decrypt() FAILED\n");
            goto err;
        }
    }


    nexus_free(buf_hdr);

    *buffer_size = crypto_buf->internal_size;

    return crypto_buf->internal_addr;

err:
    if (buf_hdr) {
        nexus_free(buf_hdr);
    }

    nexus_free(crypto_buf->internal_addr);

    return NULL;
}

static int
__serialize_gcm128_context(struct nexus_crypto_buf * crypto_buf, struct crypto_buf_hdr * buf_hdr)
{
    struct nexus_crypto_ctx * crypto_ctx  = &(crypto_buf->crypto_ctx);
    struct nexus_key        * wrapped_key = NULL;

    uint8_t * ret_ptr = NULL;



    nexus_key_set_uuid(&crypto_ctx->key, &crypto_buf->uuid);

    wrapped_key = nexus_derive_key(NEXUS_WRAPPED_128_KEY, &(crypto_ctx->key));

    if (wrapped_key == NULL) {
        log_error("could not wrap gcm128 key\n");
        return -1;
    }

    // EKEY is sealed in the buffer
    ret_ptr = nexus_key_to_buf(wrapped_key, buf_hdr->gcm_hdr.ekey, GCM128_WRAPPED_KEY_SIZE);

    if (ret_ptr == NULL) {
        log_error("Could not serialize wrapped key\n");
        goto err;
    }

    // IV
    ret_ptr = nexus_key_to_buf(&(crypto_ctx->iv), buf_hdr->gcm_hdr.iv, GCM128_IV_SIZE);

    if (ret_ptr == NULL) {
        log_error("Could not serialize IV\n");
        goto err;
    }

    // MAC
    nexus_mac_to_buf(&(crypto_ctx->mac), buf_hdr->gcm_hdr.mac);


    nexus_free_key(wrapped_key);
    nexus_free(wrapped_key);

    return 0;

err:
    nexus_free_key(wrapped_key);
    nexus_free(wrapped_key);

    return -1;
}

static struct crypto_buf_hdr *
__serialize_header(struct nexus_crypto_buf * crypto_buf)
{
    struct crypto_buf_hdr * buf_hdr = nexus_malloc(sizeof(struct crypto_buf_hdr));

    buf_hdr->magic   = NEXUS_MAGIC_V1;
    buf_hdr->version = crypto_buf->version;
    buf_hdr->size    = crypto_buf->internal_size;

    if (__serialize_gcm128_context(crypto_buf, buf_hdr)) {
        nexus_free(buf_hdr);
        log_error("Could not serialize GCM header\n");
        return NULL;
    }

    return buf_hdr;
}

int
nexus_crypto_buf_put(struct nexus_crypto_buf * crypto_buf,
                     struct nexus_mac        * mac)
{
    struct crypto_buf_hdr * buf_hdr = NULL;

    int ret = -1;



    // if we have no space allocated...
    if (crypto_buf->external_addr == NULL) {
        crypto_buf->external_size = __get_header_len() + crypto_buf->internal_size;
        crypto_buf->external_addr = buffer_layer_alloc(&crypto_buf->uuid,
                                                       crypto_buf->external_size);

        if (crypto_buf->external_addr == NULL) {
            log_error("buffer_layer_alloc FAILED\n");
            return -1;
        }
    }


    crypto_buf->version += 1;

    nexus_crypto_ctx_generate(&(crypto_buf->crypto_ctx));

    /* Generate the header for the external buffer */
    buf_hdr = __serialize_header(crypto_buf);

    if (buf_hdr == NULL) {
        log_error("serializing header FAILED\n");
        goto out;
    }


    /* Encrypt the data with the buf_hdr as AAD for authentication*/
    {
        memset(buf_hdr->gcm_hdr.mac, 0, NEXUS_MAC_SIZE);

        if (crypto_gcm_encrypt(&(crypto_buf->crypto_ctx),
                                 crypto_buf->internal_size,
                                 crypto_buf->internal_addr,
                                 crypto_buf->external_addr + __get_header_len(),
                                 mac,
                                 (uint8_t *)(buf_hdr),
                                 __get_header_len())) {
            ret = -1;
            log_error("crypto_gcm_encrypt FAILED\n");
            goto out;
        }

        nexus_mac_to_buf(&(crypto_buf->crypto_ctx.mac), buf_hdr->gcm_hdr.mac);
    }


    /* Finally copy the header out to the external buffer */
    memcpy(crypto_buf->external_addr, buf_hdr, __get_header_len());


    ret = buffer_layer_put(&crypto_buf->uuid, crypto_buf->data_size);
out:
    if (buf_hdr) {
        nexus_free(buf_hdr);
    }

    if (ret) {
        crypto_buf->version -= 1;
    }

    return ret;
}
