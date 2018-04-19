#pragma once

#include <stdbool.h>

#include <nexus_uuid.h>

#include "dirnode.h"
#include "filenode.h"


struct nexus_dentry;


typedef enum {
    NEXUS_DIRNODE,
    NEXUS_FILENODE
} nexus_metadata_type_t;


struct nexus_metadata {
    struct nexus_uuid            uuid;

    nexus_metadata_type_t        type;

    nexus_io_mode_t              mode;

    uint32_t                     version;

    size_t                       timestamp; // the last time it was read/written to disk

    size_t                       ref_count;

    bool                         is_dirty;

    union {
        struct nexus_dirnode   * dirnode;
        struct nexus_filenode  * filenode;
        void                   * object;
    };

    struct nexus_dentry        * dentry;  // dentry pointing to metadata
};



/**
 * Creates a new metadata
 * @param uuid
 * @param obj
 * @param type
 * @return metadata
 */
struct nexus_metadata *
nexus_metadata_new(struct nexus_uuid     * uuid,
                   void                  * obj,
                   nexus_metadata_type_t   type,
                   nexus_io_mode_t         mode,
                   uint32_t                version);

/**
 * Frees allocated metadata
 * @param metadata
 */
void
nexus_metadata_free(struct nexus_metadata * metadata);


int
nexus_metadata_compare(struct nexus_metadata * metadata1, struct nexus_metadata * metadata2);

/**
 * Reloads the metadata from the datastore (usually called after revalidation)
 * @return 0 on success
 */
int
nexus_metadata_reload(struct nexus_metadata * metadata, nexus_io_mode_t mode);

/**
 * Loads the metadata from the specified UUID
 * @param uuid
 * @param type
 * @param mode
 * @return metadata
 */
struct nexus_metadata *
nexus_metadata_load(struct nexus_uuid * uuid, nexus_metadata_type_t type, nexus_io_mode_t mode);

/**
 * Flushes the contents of the metadata to the datastore
 * @param metadata
 * @return 0 on success
 */
int
nexus_metadata_store(struct nexus_metadata * metadata);