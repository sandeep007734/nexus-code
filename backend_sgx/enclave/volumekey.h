#pragma once
/**
 * Each enclave will have a single global volumekey
 */

#include "nexus_key.h"
#include "sealed_buffer.h"

#include <sgx_backend_common.h>

/**
 * Generates a new enclave volumekey
 * @return 0 on success
 */
int
nexus_enclave_volumekey_generate();

/**
 * Clears the enclave volumekey
 */
void
nexus_enclave_volumekey_clear();

/**
 * Returns a sealed_buffer of the volumekey
 * @return NULL on failure
 */
struct nexus_key_buffer *
nexus_enclave_volumekey_serialize();


/**
 * parses a volumekey
 * @param sealed_buf
 * @return 0 on success
 */
int
nexus_enclave_volumekey_init(struct nexus_key_buffer * key_buffer);
