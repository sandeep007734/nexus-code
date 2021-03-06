#include "enclave_internal.h"

#include "dentry.h"
#include "hardlink_table.h"


void
__metadata_set_clean(struct nexus_metadata * metadata)
{
    metadata->is_dirty = false;
}


void
__metadata_set_dirty(struct nexus_metadata * metadata)
{
    metadata->is_dirty = true;
}

static inline void
__set_metadata_object(struct nexus_metadata * metadata, void * object)
{
    struct nexus_supernode * supernode = NULL;
    struct nexus_dirnode   * dirnode   = NULL;
    struct nexus_filenode  * filenode  = NULL;

    struct nexus_usertable * user_table      = NULL;

    struct hardlink_table  * hardlink_table  = NULL;

    struct attribute_space * attribute_space = NULL;
    struct policy_store    * policy_store    = NULL;
    struct user_profile    * user_profile    = NULL;
    struct audit_log       * audit_log       = NULL;

    switch (metadata->type) {
    case NEXUS_SUPERNODE:
        if (metadata->supernode) {
            supernode_free(metadata->supernode);
        }

        supernode = object;
        supernode->metadata = metadata;
        break;
    case NEXUS_USER_TABLE:
        if (metadata->user_table) {
            nexus_usertable_free(metadata->user_table);
        }

        user_table = object;
        user_table->metadata = metadata;
        break;
    case NEXUS_DIRNODE:
        if (metadata->dirnode) {
            dirnode_free(metadata->dirnode);
        }

        dirnode = object;
        dirnode->metadata = metadata;
        break;
    case NEXUS_FILENODE:
        if (metadata->filenode) {
            filenode_free(metadata->filenode);
        }

        filenode = object;
        filenode->metadata = metadata;
        break;
    case NEXUS_HARDLINK_TABLE:
        if (metadata->hardlink_table) {
            hardlink_table_free(metadata->hardlink_table);
        }

        hardlink_table = object;
        hardlink_table->metadata = metadata;
        break;
    case NEXUS_ATTRIBUTE_STORE:
        if (metadata->attribute_space) {
            attribute_space_free(metadata->attribute_space);
        }

        attribute_space = object;
        attribute_space->metadata = metadata;
        break;
    case NEXUS_POLICY_STORE:
        if (metadata->policy_store) {
            policy_store_free(metadata->policy_store);
        }

        policy_store = object;
        policy_store->metadata = metadata;
        break;
    case NEXUS_USER_PROFILE:
        if (metadata->user_profile) {
            user_profile_free(metadata->user_profile);
        }

        user_profile = object;
        user_profile->metadata = metadata;
        break;
    case NEXUS_AUDIT_LOG:
        if (metadata->audit_log) {
            audit_log_free(metadata->audit_log);
        }

        audit_log = object;
        audit_log->metadata = metadata;
        break;
    }

    metadata->object = object;
}

struct nexus_dentry *
metadata_get_dentry(struct nexus_metadata * metadata)
{
    if (metadata->dentry_count == 0) {
        return NULL;
    }

    return list_first_entry(&metadata->dentry_list, struct nexus_dentry, aliases);
}

struct nexus_metadata *
nexus_metadata_from_object(struct nexus_uuid     * uuid,
                           void                  * obj,
                           nexus_metadata_type_t   type,
                           nexus_io_flags_t        flags,
                           uint32_t                version)
{
    struct nexus_metadata * metadata = nexus_malloc(sizeof(struct nexus_metadata));

    metadata->type    = type;
    metadata->flags   = flags;
    metadata->version = version;

    metadata->dentry_lock = SGX_SPINLOCK_INITIALIZER;

    nexus_uuid_copy(uuid, &metadata->uuid);

    __set_metadata_object(metadata, obj);

    INIT_LIST_HEAD(&metadata->dentry_list);

    metadata->is_locked = nexus_io_in_lock_mode(flags);


    return metadata;
}

int
nexus_metadata_export_mac(struct nexus_metadata * metadata, struct nexus_mac * mac)
{
    switch (metadata->type) {
    case NEXUS_ATTRIBUTE_STORE:
        nexus_mac_copy(&metadata->attribute_space->mac, mac);
        return 0;
    }

    log_error("metadata cannot export mac\n");
    return -1;
}

struct nexus_metadata *
nexus_metadata_create(struct nexus_uuid * uuid, nexus_metadata_type_t metadata_type)
{
    void * object = NULL;

    /* we will first lock the file in the buffer layer */
    if (buffer_layer_lock(uuid, NEXUS_FCREATE)) {
        log_error("could not lock metadata file\n");
        return NULL;
    }


    switch (metadata_type) {
    case NEXUS_DIRNODE:
        object = dirnode_create(&global_supernode->root_uuid, uuid);
        break;
    case NEXUS_FILENODE:
        object = filenode_create(&global_supernode->root_uuid, uuid);
        break;
    case NEXUS_HARDLINK_TABLE:
        object = hardlink_table_create(&global_supernode->root_uuid, uuid);
        break;
    case NEXUS_ATTRIBUTE_STORE:
        object = attribute_space_create(&global_supernode->root_uuid, uuid);
        break;
    case NEXUS_POLICY_STORE:
        object = policy_store_create(&global_supernode->root_uuid, uuid);
        break;
    case NEXUS_USER_PROFILE:
        object = user_profile_create(&global_supernode->root_uuid, uuid);
        break;
    case NEXUS_AUDIT_LOG:
        object = audit_log_create(&global_supernode->root_uuid, uuid);
        break;
    default:
        log_error("cannot create object from nexus_metadata_create()\n");
        return NULL;
    }

    return nexus_metadata_from_object(uuid, object, metadata_type, NEXUS_FCREATE, 0);
}

void
nexus_metadata_free(struct nexus_metadata * metadata)
{
    switch (metadata->type) {
    case NEXUS_SUPERNODE:
        supernode_free(metadata->supernode);
        break;
    case NEXUS_USER_TABLE:
        nexus_usertable_free(metadata->user_table);
        break;
    case NEXUS_DIRNODE:
        dirnode_free(metadata->dirnode);
        break;
    case NEXUS_FILENODE:
        filenode_free(metadata->filenode);
        break;
    case NEXUS_HARDLINK_TABLE:
        hardlink_table_free(metadata->hardlink_table);
        break;
    case NEXUS_ATTRIBUTE_STORE:
        attribute_space_free(metadata->attribute_space);
        break;
    case NEXUS_POLICY_STORE:
        policy_store_free(metadata->policy_store);
        break;
    case NEXUS_USER_PROFILE:
        user_profile_free(metadata->user_profile);
        break;
    case NEXUS_AUDIT_LOG:
        audit_log_free(metadata->audit_log);
        break;
    }

    if (metadata->dentry_count) {
        struct list_head * curr = NULL;
        struct list_head * pos = NULL;

        list_for_each_safe(curr, pos, &metadata->dentry_list) {
            struct nexus_dentry * dentry = NULL;

            dentry = list_entry(curr, struct nexus_dentry, aliases);

            dentry_invalidate(dentry);
        }

        metadata->dentry_count = 0;
    }

    memset(metadata, 0, sizeof(struct nexus_metadata));

    nexus_free(metadata);
}

static void *
__read_object(struct nexus_uuid     * uuid,
              nexus_metadata_type_t   type,
              nexus_io_flags_t        flags,
              uint32_t              * version)
{
    void                    * object     = NULL;

    struct nexus_crypto_buf * crypto_buf = NULL;


    if (type == NEXUS_FILENODE) {
        flags |= NEXUS_IO_FNODE;
    }

    crypto_buf = nexus_crypto_buf_create(uuid, flags);

    if (crypto_buf == NULL) {
        log_error("could not read crypto_buf\n");
        return NULL;
    }

    *version = nexus_crypto_buf_version(crypto_buf);

    if (*version == 0) {
        switch (type) {
        case NEXUS_DIRNODE:
            object = dirnode_create(&global_supernode->root_uuid, uuid);
            break;
        case NEXUS_FILENODE:
            object = filenode_create(&global_supernode->root_uuid, uuid);
            break;
        case NEXUS_ATTRIBUTE_STORE:
            object = attribute_space_create(&global_supernode->root_uuid, uuid);
            break;
        case NEXUS_POLICY_STORE:
            object = policy_store_create(&global_supernode->root_uuid, uuid);
            break;
        case NEXUS_USER_PROFILE:
            object = user_profile_create(&global_supernode->root_uuid, uuid);
            break;
        case NEXUS_AUDIT_LOG:
            object = audit_log_create(&global_supernode->root_uuid, uuid);
            break;
        default:
            log_error("metadata cannot be version 0\n");
            break;
        }
    } else {
        switch (type) {
        case NEXUS_DIRNODE:
            object = dirnode_from_crypto_buf(crypto_buf, flags);
            break;
        case NEXUS_FILENODE:
            object = filenode_from_crypto_buf(crypto_buf, flags);
            break;
        case NEXUS_SUPERNODE:
            object = supernode_from_crypto_buf(crypto_buf, flags);
            break;
        case NEXUS_HARDLINK_TABLE:
            object = hardlink_table_from_crypto_buf(crypto_buf, flags);
            break;
        case NEXUS_ATTRIBUTE_STORE:
            object = attribute_space_from_crypto_buf(crypto_buf);
            break;
        case NEXUS_POLICY_STORE:
            object = policy_store_from_crypto_buf(crypto_buf);
            break;
        case NEXUS_USER_PROFILE:
            object = user_profile_from_crypto_buf(crypto_buf);
            break;
        case NEXUS_USER_TABLE:
            object = nexus_usertable_from_crypto_buf(crypto_buf);
            break;
        case NEXUS_AUDIT_LOG:
            object = audit_log_from_crypto_buf(crypto_buf);
            break;
        }
    }

    nexus_crypto_buf_free(crypto_buf);

    return object;
}

bool
nexus_metadata_has_changed(struct nexus_metadata * metadata)
{
    bool has_changed;

    buffer_layer_revalidate(&metadata->uuid, &has_changed);

    return has_changed;
}

int
nexus_metadata_revalidate(struct nexus_metadata * metadata,
                          nexus_io_flags_t        flags,
                          bool                  * has_changed)
{
    if (metadata->is_invalid) {
        return nexus_metadata_reload(metadata, flags);
    }

    buffer_layer_revalidate(&metadata->uuid, has_changed);

    if (*has_changed) {
        return nexus_metadata_reload(metadata, flags);
    }

    if (nexus_io_in_lock_mode(flags)) {
        return nexus_metadata_lock(metadata, flags);
    }

    return 0;
}

int
nexus_metadata_reload(struct nexus_metadata * metadata, nexus_io_flags_t flags)
{
    void    * object = NULL;

    uint32_t version = 0;


    object = __read_object(&metadata->uuid, metadata->type, flags, &version);

    if (object == NULL) {
        log_error("could not reload metadata object\n");
        return -1;
    }

    metadata->flags = flags;
    __set_metadata_object(metadata, object);

    metadata->is_locked = nexus_io_in_lock_mode(flags);

    metadata->is_invalid = false;

    metadata->is_dirty = false;

    metadata->version = version;

    return 0;
}

struct nexus_metadata *
nexus_metadata_load(struct nexus_uuid * uuid, nexus_metadata_type_t type, nexus_io_flags_t flags)
{
    void    * object = NULL;

    uint32_t version = 0;


    object = __read_object(uuid, type, flags, &version);

    if (object == NULL) {
        log_error("reading metadata object FAILED\n");
        return NULL;
    }

    return nexus_metadata_from_object(uuid, object, type, flags, version);
}

int
nexus_metadata_store(struct nexus_metadata * metadata)
{
    int ret = -1;

    if (!metadata->is_dirty && !(metadata->flags & NEXUS_FCREATE)) {
        return 0;
    }


    switch (metadata->type) {
    case NEXUS_SUPERNODE:
        ret = supernode_store(metadata->supernode, metadata->version, NULL);
        break;
    case NEXUS_DIRNODE:
        ret = dirnode_store(metadata->dirnode, metadata->version, NULL);
        break;
    case NEXUS_FILENODE:
        ret = filenode_store(metadata->filenode, metadata->version, NULL);
        break;
    case NEXUS_HARDLINK_TABLE:
        ret = hardlink_table_store(metadata->hardlink_table, metadata->version, NULL);
        break;
    case NEXUS_ATTRIBUTE_STORE:
        ret = attribute_space_store(metadata->attribute_space, metadata->version, NULL);
        break;
    case NEXUS_POLICY_STORE:
        ret = policy_store_store(metadata->policy_store, metadata->version, NULL);
        break;
    case NEXUS_USER_PROFILE:
        ret = user_profile_store(metadata->user_profile, metadata->version, NULL);
        break;
    case NEXUS_USER_TABLE:
        ret = nexus_usertable_store(metadata->user_table, metadata->version, NULL);
        break;
    case NEXUS_AUDIT_LOG:
        ret = audit_log_store(metadata->audit_log, metadata->version, NULL);
        break;
    default:
        log_error("metadata->type UNKNOWN\n");
        return -1;
    }

    if (ret == 0) {
        __metadata_set_clean(metadata);
        metadata->version += 1;
        metadata->is_locked = false;
    } else {
        nexus_io_flags_t flags = 0;
        metadata->is_invalid = true;

        if (buffer_layer_lock_status(&metadata->uuid, &flags) == 0) {
            metadata->is_locked = nexus_io_in_lock_mode(flags);
        } else {
            metadata->is_locked = false;
        }
    }

#if 0
    if (metadata->audit_log_metadata) {
        struct audit_log * audit_log = metadata->audit_log_metadata->audit_log;

        if (audit_log_complete_write(audit_log, metadata->version)) {
            log_error("audit_log_complete_write() FAILED\n");
            goto out_err;
        }

        if (__nexus_metadata_sync(metadata->audit_log_metadata)) {
            log_error("could not sync audit_log\n");
            goto out_err;
        }

        metadata->audit_log_metadata = NULL;
    }
#endif

    return ret;
out_err:
    metadata->audit_log_metadata = NULL;

    return -1;
}

int
__nexus_metadata_sync(struct nexus_metadata * metadata)
{
    nexus_io_flags_t old_flags = metadata->flags;

    if (!metadata->is_locked) {
        if (nexus_metadata_lock(metadata, NEXUS_FWRITE)) {
            log_error("nexus_metadata_lock() FAILED\n");
            goto out_err;
        }
    }

    if (nexus_metadata_store(metadata)) {
        log_error("nexus_metadata_store() FAILED\n");
        goto out_err;
    }

    // reopen in old mode
    if (nexus_io_in_lock_mode(old_flags)) {
        if (nexus_metadata_lock(metadata, old_flags)) {
            log_error("nexus_metadata_lock() FAILED\n");
            goto out_err;
        }
    }

    return 0;
out_err:
    return -1;
}

struct nexus_metadata *
nexus_metadata_get(struct nexus_metadata * metadata)
{
    if (metadata) {
        metadata->ref_count += 1;
    }

    return metadata;
}

/**
 * Decrements the ref count of the metadata object
 * @param metadata
 */
void
nexus_metadata_put(struct nexus_metadata * metadata)
{
    metadata->ref_count -= 1;
}


int
nexus_metadata_lock(struct nexus_metadata * metadata, nexus_io_flags_t flags)
{
    if (metadata->type == NEXUS_FILENODE) {
        flags |= NEXUS_IO_FNODE;
    }

    if (buffer_layer_lock(&metadata->uuid, flags)) {
        log_error("buffer_layer_lock() FAILED\n");
        return -1;
    }

    metadata->is_locked = true;

    return 0;
}

void
nexus_metadata_unlock(struct nexus_metadata * metadata)
{
    if (metadata->is_locked) {
        buffer_layer_unlock(&metadata->uuid);
        metadata->is_locked = false;
    }
}

void
nexus_metadata_reset(struct nexus_metadata * metadata)
{
    nexus_metadata_unlock(metadata);
    nexus_metadata_reload(metadata, NEXUS_FREAD);
}

int
nexus_metadata_verify_uuids(struct nexus_dentry * dentry)
{
    // make sure the dentry's real uuid matches the metadata's uuid
    if (nexus_uuid_compare(&dentry->metadata->uuid, &dentry->link_uuid)) {
        return -1;
    }

    return 0;
}

struct attribute_table *
metadata_get_attribute_table(struct nexus_metadata * metadata)
{
    if (metadata->type == NEXUS_DIRNODE) {
        return metadata->dirnode->attribute_table;
    } else if (metadata->type == NEXUS_FILENODE) {
        return metadata->filenode->attribute_table;
    } else if (metadata->type == NEXUS_USER_PROFILE) {
        return metadata->user_profile->attribute_table;
    } else {
        log_error("incorrect metadata type\n");
        return NULL;
    }
}

bool
metadata_has_audit_log(struct nexus_metadata * metadata)
{
    struct attribute_table * attribute_table = metadata_get_attribute_table(metadata);

    if (attribute_table == NULL) {
        log_error("could not get attribute table\n");
        return -1;
    }

    return attribute_table_has_audit_log(attribute_table);
}

int
metadata_create_audit_log(struct nexus_metadata * metadata)
{
    struct nexus_uuid audit_log_uuid;

    struct attribute_table * attribute_table = metadata_get_attribute_table(metadata);

    if (attribute_table == NULL) {
        log_error("could not get attribute table\n");
        return -1;
    }

    if (attribute_table_has_audit_log(attribute_table)) {
        return 0;
    }

    // create the audit log and write to disk
    {
        struct nexus_metadata * audit_log_metadata = NULL;

        nexus_uuid_gen(&audit_log_uuid);

        audit_log_metadata = nexus_metadata_create(&audit_log_uuid, NEXUS_AUDIT_LOG);

        if (nexus_metadata_store(audit_log_metadata)) {
            nexus_metadata_free(audit_log_metadata);
            goto out_err;
        }

        nexus_metadata_free(audit_log_metadata);
    }

    attribute_table_set_audit_log(attribute_table, &audit_log_uuid);

    __metadata_set_dirty(metadata);

    // check the file is in write-mode, flush it, and re-open
    if (__nexus_metadata_sync(metadata)) {
        log_error("__nexus_metadata_sync() FAILED\n");
        goto out_err;
    }

    return 0;
out_err:
    return -1;
}


struct nexus_metadata *
metadata_get_audit_log(struct nexus_metadata * metadata, nexus_io_flags_t flags)
{
    struct attribute_table * attribute_table = metadata_get_attribute_table(metadata);

    struct nexus_uuid audit_log_uuid;

    if (attribute_table == NULL) {
        log_error("could not get attribute table\n");
        return -1;
    }

    if (attribute_table_get_audit_log(attribute_table, &audit_log_uuid)) {
        log_error("attribute table does not have audit log\n");
        return NULL;
    }

    return nexus_vfs_load(&audit_log_uuid, NEXUS_AUDIT_LOG, flags);
}
