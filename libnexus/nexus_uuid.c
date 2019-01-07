/* 
 * Copyright (c) 2017, Jack Lange <jacklange@cs.pitt.edu>
 * All rights reserved.
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "PETLAB_LICENSE".
 */
#include <stdint.h>
#include <uuid/uuid.h>

#include <nexus_uuid.h>
#include <nexus_encode.h>
#include <nexus_util.h>
#include <nexus_log.h>

#include "xxhash.h"


int
nexus_uuid_gen(struct nexus_uuid * uuid)
{    
    uuid_generate(uuid->raw);

    return 0;
}

struct nexus_uuid *
nexus_uuid_clone(struct nexus_uuid * uuid)
{
    struct nexus_uuid * new_uuid = NULL;

    new_uuid = calloc(sizeof(struct nexus_uuid), 1);

    if (new_uuid == NULL) {
        log_error("Could not allocate new uuid\n");
        return NULL;
    }

    memcpy(new_uuid->raw, uuid->raw, NEXUS_UUID_SIZE);

    return new_uuid;
}

int
nexus_uuid_copy(struct nexus_uuid * src_uuid, struct nexus_uuid * dst_uuid)
{
    memcpy(dst_uuid->raw, src_uuid->raw, NEXUS_UUID_SIZE);

    return 0;
}

int
nexus_uuid_compare(struct nexus_uuid * uuid1, struct nexus_uuid * uuid2)
{
    return memcmp(uuid1->raw, uuid2->raw, NEXUS_UUID_SIZE);
}

char *
nexus_uuid_to_base64(struct nexus_uuid * uuid)
{
    char * base64_str = NULL;

    base64_str = nexus_base64_encode(uuid->raw, NEXUS_UUID_SIZE);

    return base64_str;
}

int
nexus_uuid_from_base64(struct nexus_uuid * uuid, char * base64_str)
{
    uint8_t * tmp_buf = NULL;
    uint32_t  size    = 0;

    int ret = 0;

    ret = nexus_base64_decode(base64_str, &tmp_buf, &size);

    if (ret == -1) {
        log_error("Could not decode uuid from (%s)\n", base64_str);
        return -1;
    }

    if (size != NEXUS_UUID_SIZE) {
        nexus_free(tmp_buf);
        log_error("Decoded a UUID with invalid length (%d)\n", size);
        return -1;
    }

    memcpy(uuid->raw, tmp_buf, NEXUS_UUID_SIZE);

    nexus_free(tmp_buf);

    return 0;
}

char *
nexus_uuid_to_alt64(struct nexus_uuid * uuid)
{
    char * alt64_str = NULL;

    alt64_str = nexus_alt64_encode(uuid->raw, NEXUS_UUID_SIZE);

    return alt64_str;
}

int
nexus_uuid_from_alt64(struct nexus_uuid * uuid, char * alt64_str)
{
    uint8_t * tmp_buf = 0;
    uint32_t  size    = 0;

    int ret = 0;

    ret = nexus_alt64_decode(alt64_str, &tmp_buf, &size);

    if (ret == -1) {
        log_error("Could not decode uuid from (%s)\n", alt64_str);
        return -1;
    }

    if (size != NEXUS_UUID_SIZE) {
        nexus_free(tmp_buf);
        log_error("Decoded a UUID with invalid length (%d) (uuid_str=%s)\n", size, alt64_str);
        return -1;
    }

    memcpy(uuid->raw, tmp_buf, NEXUS_UUID_SIZE);

    nexus_free(tmp_buf);

    return 0;
}

char *
nexus_uuid_to_hex(struct nexus_uuid * uuid)
{
    return nexus_hex_encode(uuid->raw, NEXUS_UUID_SIZE);
}

int
nexus_uuid_from_hex(struct nexus_uuid * uuid, char * hex_str)
{
    uint8_t * uuid_buf = NULL;
    uint32_t  uuid_len = 0;

    if (nexus_hex_decode(hex_str, &uuid_buf, &uuid_len)) {
        log_error("nexus_hex_decode FAILED\n");
        return -1;
    }

    if (uuid_len != NEXUS_UUID_SIZE) {
        nexus_free(uuid_buf);
        log_error("Decoded a UUID with invalid length (%d) (uuid_str=%s)\n", uuid_len, hex_str);
        return -1;
    }

    memcpy(uuid->raw, uuid_buf, NEXUS_UUID_SIZE);

    nexus_free(uuid_buf);

    return 0;
}



uint32_t
nexus_uuid_hash(struct nexus_uuid * uuid)
{
    return (uint32_t)(XXH32(uuid, sizeof(struct nexus_uuid), 0));
}
