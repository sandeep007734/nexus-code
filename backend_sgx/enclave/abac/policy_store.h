#pragma once

#include "abac_internal.h"


struct policy_store {
    struct nexus_uuid       my_uuid;
    struct nexus_uuid       root_uuid;

    uint32_t                rules_count;

    struct nexus_mac        mac;

    struct mac_and_version  attribute_store_macversion;

    struct nexus_list       rules_list;


    struct nexus_metadata * metadata;
};


struct policy_store *
policy_store_create(struct nexus_uuid * root_uuid, struct nexus_uuid * uuid);

void
policy_store_free(struct policy_store * policy_store);


struct policy_store *
policy_store_from_crypto_buf(struct nexus_crypto_buf * crypto_buffer);

int
policy_store_store(struct policy_store * policy_store, uint32_t version, struct nexus_mac * mac);



/// adds a policy to the store and returns the created policy rule
int
policy_store_add(struct policy_store * policy_store, struct policy_rule * policy_rule);

/// tries to delete a policy, returns -1 on FAILURE
int
policy_store_del(struct policy_store * policy_store, struct nexus_uuid * rule_uuid);


/**
 * Returns a list of policies matching the given action
 */
struct nexus_list *
policy_store_select_policies(struct policy_store * policy_store, perm_type_t permission);
