#pragma once
#include <nexus_uuid.h>

/**
 * The buffer layer handles all the metadata operations with untrusted
 * code.
 */

int
buffer_layer_init();

int
buffer_layer_exit();

uint8_t *
buffer_layer_alloc(struct nexus_uuid * uuid, size_t size);

int
buffer_layer_lock(struct nexus_uuid * uuid, nexus_io_flags_t flags);

int
buffer_layer_unlock(struct nexus_uuid * uuid);

/**
 * Used for querying lock status of buffer layer. Used mostly in metadata_store
 * to handle failed store operations.
 * @param uuid
 * @param flags_out
 * @return -1 if buffer is not in found
 */
int
buffer_layer_lock_status(struct nexus_uuid * uuid, nexus_io_flags_t * flags_out);

/**
 * Removes the metadata from the buffer layer cache
 * @param uuid
 */
void
buffer_layer_evict(struct nexus_uuid * uuid);

/**
 * Checks if the metadata has changed since the last time the buffer
 * checked the backend.
 *
 * @return -1 if the check could not be done (ocall failure)
 */
int
buffer_layer_revalidate(struct nexus_uuid * uuid, bool * should_reload);

/**
 * Acquires a reference to an externally allocated buffer
 * @param uuid
 * @param size
 * @return the address to the external buffer.
 */
void *
buffer_layer_get(struct nexus_uuid * uuid, nexus_io_flags_t flags, size_t * size);

/**
 * Drops reference to a specified buffer
 * @param uuid
 * @param buffer
 * @param buflen
 * @return 0 on success
 */
int
buffer_layer_put(struct nexus_uuid * uuid, size_t data_size);

/**
 * Creates an empty file on the datastore
 * @param uuid
 */
int
buffer_layer_new(struct nexus_uuid * uuid);

/**
 * Deletes a metadata buffer
 * @param uuid
 */
int
buffer_layer_delete(struct nexus_uuid * uuid);
