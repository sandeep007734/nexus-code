#include "abac_internal.h"

#include "../metadata.h"

static struct nexus_metadata * attribute_store_metadata = NULL;

static struct nexus_metadata * current_user_profile_metadata = NULL;

static struct abac_superinfo   global_abac_superinfo;


int
abac_global_export_macversion(struct mac_and_version * macversion)
{
    // TODO
    return -1;
}


static inline struct nexus_uuid *
__attribute_store_uuid()
{
    return &global_supernode->abac_superinfo.attribute_store_uuid;
}

static inline struct nexus_uuid *
__policy_store_uuid()
{
    return &global_supernode->abac_superinfo.policy_store_uuid;
}

struct attribute_store *
abac_acquire_attribute_store(nexus_io_flags_t flags)
{
    struct nexus_uuid * uuid = __attribute_store_uuid();

    bool has_changed;


    if (attribute_store_metadata == NULL) {
        attribute_store_metadata = nexus_metadata_load(uuid, NEXUS_ATTRIBUTE_STORE, flags);

        if (attribute_store_metadata == NULL) {
            log_error("nexus_metadata_load() FAILED\n");
            return NULL;
        }

        return attribute_store_metadata->attribute_store;
    }

    if (nexus_metadata_revalidate(attribute_store_metadata, flags, &has_changed)) {
        log_error("nexus_metadata_revalidate() FAILED\n");
        return NULL;
    }

    return attribute_store_metadata->attribute_store;
}

int
abac_flush_attribute_store()
{
    return nexus_metadata_store(attribute_store_metadata);
}

void
abac_release_attribute_store()
{
    nexus_metadata_unlock(attribute_store_metadata);
}

int
abac_runtime_mount()
{
    if (abac_acquire_attribute_store(NEXUS_FREAD) == NULL) {
        log_error("could not load attribute store\n");
        return -1;
    }

    return 0;
}

int
abac_runtime_create(struct abac_superinfo * dst_abac_superinfo)
{
    struct nexus_metadata * tmp_metadata = NULL;

    struct nexus_uuid * policy_store_uuid    = &dst_abac_superinfo->policy_store_uuid;
    struct nexus_uuid * attribute_store_uuid = &dst_abac_superinfo->attribute_store_uuid;


    nexus_uuid_gen(policy_store_uuid);
    nexus_uuid_gen(attribute_store_uuid);

    // TODO write the policy store

    // create the attribute store metadata
    {
        tmp_metadata = nexus_metadata_create(attribute_store_uuid, NEXUS_ATTRIBUTE_STORE);
        if (tmp_metadata == NULL) {
            log_error("nexus_metadata_create() FAILED\n");
            goto out_err;
        }

        if (nexus_metadata_store(tmp_metadata)) {
            nexus_metadata_free(tmp_metadata);
            log_error("nexus_metadata_store() FAILED\n");
            goto out_err;
        }

        nexus_metadata_free(tmp_metadata);
    }

    return 0;
out_err:
    return -1;
}

void
abac_runtime_destroy()
{
    // TODO
}

attribute_type_t
attribute_type_from_str(char * attribute_type_str)
{
    if (strncmp("user", attribute_type_str, 5) == 0) {
        return USER_ATTRIBUTE_TYPE;
    } else if (strncmp("object", attribute_type_str, 5) == 0) {
        return OBJECT_ATTRIBUTE_TYPE;
    }

    return -1;
}

int
attribute_type_to_str(attribute_type_t attribute_type, char * buffer_out, size_t buflen)
{
    char * attr_str = NULL;

    switch(attribute_type) {
    case USER_ATTRIBUTE_TYPE:
        attr_str = "user";
        break;
    case OBJECT_ATTRIBUTE_TYPE:
        attr_str = "object";
        break;
    default:
        log_error("could not find attribute_type from argument\n");
        return -1;
    }

    strncpy(buffer_out, attr_str, buflen);

    return 0;
}


// TODO have cache interface for user_profiles

struct user_profile *
abac_get_user_profile(char * username, nexus_io_flags_t flags)
{
    struct nexus_profile   * rst_profile = NULL;
    struct nexus_user      * rst_user    = NULL;
    struct nexus_usertable * usertable   = abac_global_get_usertable(NEXUS_FREAD);

    if (usertable == NULL) {
        log_error("nexus_global_get_usertable() FAILED\n");
        return NULL;
    }

    rst_user = nexus_usertable_find_name(usertable, username);

    if (rst_user == NULL) {
        log_error("could not find user (%s)\n", username);
        goto err;
    }

    // now load the user profile using the uuid
    {
        struct nexus_metadata  * rst_profile_metadata = NULL;

        rst_profile_metadata = nexus_metadata_load(&rst_user->user_uuid, NEXUS_USER_PROFILE, flags);

        if (rst_profile_metadata == NULL) {
            log_error("could not load user profile metadata\n");
            goto err;
        }

        rst_profile = rst_profile_metadata->user_profile;
    }

    abac_global_put_usertable(usertable);

    return rst_profile;
err:
    abac_global_put_usertable(usertable);

    return -1;
}

int
abac_put_user_profile(struct user_profile * user_profile)
{
    if (user_profile != global_supernode->usertable) {
        log_error("user_profile passed is not volume's user table\n");
        return -1;
    }

    if (nexus_metadata_store(user_profile)) {
        log_error("nexus_metadata_store() FAILED\n");
        return -1;
    }

    nexus_metadata_unlock(user_profile->metadata);

    nexus_metadata_free(user_profile->metadata);

    return 0;
}

int
abac_create_user_profile(struct nexus_uuid * user_uuid)
{
    struct nexus_metadata * tmp_metadata = NULL;

    tmp_metadata = nexus_metadata_create(user_uuid, NEXUS_ATTRIBUTE_STORE);
    if (tmp_metadata == NULL) {
        log_error("nexus_metadata_create() FAILED\n");
        goto out_err;
    }

    if (nexus_metadata_store(tmp_metadata)) {
        nexus_metadata_free(tmp_metadata);
        log_error("nexus_metadata_store() FAILED\n");
        goto out_err;
    }

    nexus_metadata_free(tmp_metadata);

    return 0;
out_err:
    return -1;
}

int
abac_del_user_profile(struct nexus_uuid * user_uuid)
{
    return buffer_layer_delete(user_uuid);
}


struct nexus_usertable *
abac_global_get_usertable(nexus_io_flags_t flags)
{
    struct nexus_supernode * global_supernode = nexus_vfs_acquire_supernode(flags);

    if (global_supernode == NULL) {
        log_error("could not acquire global supernode\n");
        return NULL;
    }

    return global_supernode->usertable;
}

int
abac_global_put_usertable(struct nexus_usertable * usertable)
{
    if (nexus_metadata_store(global_supernode_metadata)) {
        log_error("nexus_metadata_store() FAILED\n");
        return -1;
    }

    nexus_vfs_release_supernode();
}
