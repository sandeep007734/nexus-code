/* 
 * Copyright (c) 2017, Jack Lange <jacklange@cs.pitt.edu>
 * All rights reserved.
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "PETLAB_LICENSE".
 */
#pragma once

#include <stdint.h>
#include <stdio.h>

typedef enum {
    NEXUS_INVALID_KEY       = 0,
    NEXUS_MBEDTLS_PUB_KEY   = 1,
    NEXUS_MBEDTLS_PRV_KEY   = 2,
    NEXUS_RAW_128_KEY       = 3,
    NEXUS_RAW_256_KEY       = 4,
    NEXUS_WRAPPED_128_KEY   = 5,
    NEXUS_WRAPPED_256_KEY   = 6,
    NEXUS_SEALED_VOLUME_KEY = 7
} nexus_key_type_t;

struct nexus_key {
    nexus_key_type_t type;

    struct nexus_uuid * uuid;  // associated UUID

    void * key;
};


/* Creates a new random key */
struct nexus_key *
nexus_create_key(nexus_key_type_t key_type);


/* Initializes 'key' with a random key */
int
nexus_generate_key(struct nexus_key * key, nexus_key_type_t key_type);

/* initializes key to a type, call nexus_generate_key() to fill contents */
int
nexus_init_key(struct nexus_key * key, nexus_key_type_t key_type);

int
nexus_key_set_uuid(struct nexus_key * key, struct nexus_uuid * uuid);


/* Use this to generate a public key from a private key */
struct nexus_key *
nexus_derive_key(nexus_key_type_t key_type, struct nexus_key * src_key);

/* Use this to generate a public key from a private key */
int
__nexus_derive_key(struct nexus_key * new_key,
                   nexus_key_type_t   key_type,
                   struct nexus_key * src_key);

int
nexus_copy_key(struct nexus_key * src_key, struct nexus_key * dst_key);

struct nexus_key *
nexus_clone_key(struct nexus_key * src_key);


void
nexus_free_key(struct nexus_key * key);


/**
 * Returns the size of the key in bytes.
 *
 * @return -1 for invalid key types
 */
int
nexus_key_bytes(struct nexus_key * key);

int
nexus_key_bits(struct nexus_key * key);



/* Return Value: 
 *    NULL on error, otherwise a pointer to the buffer serialized into
 *    If dst_buf is NULL, then a new buffer is allocated
 */
uint8_t *
nexus_key_to_buf(struct nexus_key * key, uint8_t * dst_buf, size_t dst_size);

int
__nexus_key_from_buf(struct nexus_key * key,
                     nexus_key_type_t   key_type,
                     uint8_t          * src_buf,
                     size_t             src_buflen);

struct nexus_key *
nexus_key_from_buf(nexus_key_type_t key_type, uint8_t * src_buf, size_t src_size);

struct nexus_key *
nexus_key_from_file(nexus_key_type_t key_type, char * key_path);

int
__nexus_key_from_file(struct nexus_key * key, nexus_key_type_t key_type, char * key_path);

int
nexus_key_to_file(struct nexus_key * key, char * file_path);

/* Creates a new key from file contents */
struct nexus_key *
nexus_key_from_str(nexus_key_type_t key_type, char * key_str);

/* Copies file contents into existing key structure */
int
__nexus_key_from_str(struct nexus_key * key, nexus_key_type_t key_type, char * key_str);

char *
nexus_key_to_str(struct nexus_key * key);


char *
nexus_key_type_to_str(nexus_key_type_t type);

nexus_key_type_t
nexus_key_type_from_str(char * type_str);

static inline nexus_key_type_t
nexus_get_key_type(struct nexus_key * key) {
    return key->type;
}
