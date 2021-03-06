#pragma once

/**
 * a small ACL-based permission system
 * @author Judicael Briand
 */

#include <stdint.h>

#include "user.h"

#include <nexus_list.h>

struct nexus_acl {
    size_t            count;

    struct nexus_list acls;
};

typedef enum {
    NEXUS_PERM_READ   = 0x0001,
    NEXUS_PERM_WRITE  = 0x0002,
    NEXUS_PERM_CREATE = 0x0004,
    NEXUS_PERM_DELETE = 0x0008,
    NEXUS_PERM_LOOKUP = 0x0010,
    NEXUS_PERM_ADMIN  = 0x0020
} nexus_perm_t;

/**
 * an ACL entry associates a user id to a permission
 */
struct nexus_acl_entry {
    nexus_perm_t perm;
    nexus_uid_t  uid;
};

int
__nexus_acl_from_buffer(struct nexus_acl * nexus_acl, uint8_t * buffer, size_t buflen);

/**
 * Serializes the ACL unto a buffer
 * @param nexus_acl
 * @param buffer
 * @return 0 on success
 */
int
nexus_acl_to_buffer(struct nexus_acl * nexus_acl, uint8_t * buffer);

/**
 * initalizes a nexus ACL
 * @param nexus_acl
 */
void
nexus_acl_init(struct nexus_acl * nexus_acl);

/**
 * Frees resources allocated in the ACL. Calls free(nexus_acl)
 * @param nexus_acl
 */
void
nexus_acl_free(struct nexus_acl * nexus_acl);

/**
 * Returns the buffer size necessary for serialization of the ACL
 * @param nexus_acl
 * @return the size
 */
size_t
nexus_acl_size(struct nexus_acl * nexus_acl);

/**
 * Adds a new ACL entry
 *
 * @param nexus_acl
 * @param uid the user's ID
 * @param rights
 *
 * @return 0 on success
 */
int
nexus_acl_set(struct nexus_acl * nexus_acl, nexus_uid_t uid, nexus_perm_t perm);

/**
 * Removes ACL entry from user
 * @param nexus_acl
 */
int
nexus_acl_unset(struct nexus_acl * nexus_acl, nexus_uid_t uid, nexus_perm_t perm);

/**
 * Removes a user's entry from the ACL, effectively unsetting all rights
 * @param nexus_acl
 * @param uid
 */
int
nexus_acl_remove(struct nexus_acl * nexus_acl, nexus_uid_t uid);

/**
 * Checks if the currently authenticated user has the right to perform a given action
 * @param nexus_acl
 * @param perm
 * @return true on success
 */
bool
nexus_acl_is_authorized(struct nexus_acl * nexus_acl, nexus_perm_t perm);
