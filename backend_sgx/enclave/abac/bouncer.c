#include "abac_internal.h"

#include "./datalog-engine/engine.h"

#include "../enclave_internal.h"

#include <libnexus_trusted/nexus_lru.h>
#include <libnexus_trusted/nexus_uuid.h>
#include <libnexus_trusted/hashmap.h>

#include "system_functions.h"

#include "value.h"
#include "fact.h"
#include "db.h"


struct abac_request {
    struct policy_store     * policy_store;
    struct attribute_store  * attribute_store;

    struct nexus_metadata   * metadata;
    struct __cached_element * object_element;

    struct user_profile     * user_profile;

    perm_type_t               perm_type;
};


static struct nexus_lru        * object_elements_map  = NULL;

static struct __cached_element * user_profile_element = NULL;
static struct __cached_element * policy_rules_element = NULL;

static void
__destroy_abac_request(struct abac_request * abac_req);

static struct __cached_element *
__object_element_upsert(struct nexus_uuid * uuid);



static int
__register_fact_with_db(struct __cached_element * cached_element,
                        struct __cached_fact    * cached_fact,
                        char                    * value)
{
    if (cached_fact->is_inserted) {
        if (strncmp(cached_fact->value, value, ATTRIBUTE_VALUE_SIZE) == 0) {
            return 0;
        }

        // retract from datalog engine and update the value
        if (db_retract_fact(cached_fact)) {
            log_error("db_retract_fact() FAILED\n");
            return -1;
        }

        nexus_free(cached_fact->value);
        cached_fact->value = value;
        cached_fact->is_inserted = false;
    }

    if (db_assert_fact(cached_fact)) {
        log_error("db_retract_fact() FAILED\n");
        return -1;
    }

    cached_fact->is_inserted = true;

    return 0;
}


// --[[ system facts

static int
__insert_system_functions(void                    * user_or_object,
                          struct __cached_element * cached_element,
                          struct nexus_list       * sysfacts_list)
{
    struct nexus_list_iterator * iter = list_iterator_new(sysfacts_list);

    struct __cached_fact * cached_fact = NULL;

    while (list_iterator_is_valid(iter)) {
        struct __sys_func * sys_func = list_iterator_get(iter);

        const char * name      = sys_func_get_name(sys_func);
        char *       new_value = system_function_run(sys_func, user_or_object);

        if (new_value == NULL) {
            // TODO report here
            goto next;
        }

        // get the cached fact and compare its value
        cached_fact = cached_element_find_name_fact(cached_element, (char *)name);

        if (cached_fact) {
            // if the value is unchanged, let's skip
            if ((strncmp(new_value, cached_fact->value, ATTRIBUTE_VALUE_SIZE) == 0)) {
                nexus_free(new_value);
                goto next;
            }
        } else {
            cached_fact = cached_element_put_name_fact(cached_element, name, new_value);
        }

        if (__register_fact_with_db(cached_element, cached_fact, new_value)) {
            list_iterator_free(iter);
            log_error("__register_fact_with_db() FAILED\n");
            return -1;
        }

next:
        list_iterator_next(iter);
    }

    list_iterator_free(iter);

    return 0;
}

static int
__abac_request_insert_sysfuncs(struct abac_request * abac_req)
{
    struct nexus_list * usr_sysfacts = NULL;
    struct nexus_list * obj_sysfacts = NULL;

    int ret = -1;


    usr_sysfacts = system_function_export_sysfuncs(USER_FUNCTION);

    if (__insert_system_functions(abac_req->user_profile, user_profile_element, usr_sysfacts)) {
        log_error("could not insert user system functions\n");
        goto out_err;
    }

    if (abac_req->metadata) {
        obj_sysfacts = system_function_export_sysfuncs(OBJECT_FUNCTION);

        if (__insert_system_functions(abac_req->metadata, abac_req->object_element, obj_sysfacts)) {
            log_error("could not insert user system functions\n");
            goto out_err;
        }
    }


    ret = 0;
out_err:
    if (usr_sysfacts) {
        nexus_list_destroy(usr_sysfacts);
        nexus_free(usr_sysfacts);
    }

    if (obj_sysfacts) {
        nexus_list_destroy(obj_sysfacts);
        nexus_free(obj_sysfacts);
    }

    return ret;
}

// --]] system facts


// --[[ attributes

static int
__register_attribute_fact(struct __cached_element * cached_element,
                          struct nexus_uuid       * attr_uuid,
                          char                    * name,
                          char                    * value)
{
    struct __cached_fact * cached_fact = NULL;

    cached_fact = cached_element_find_uuid_fact(cached_element, attr_uuid);

    if (cached_fact == NULL) {
        cached_fact = cached_element_put_uuid_fact(cached_element, attr_uuid, name, value);
    }

    return __register_fact_with_db(cached_element, cached_fact, value);
}

static int
__insert_attribute_table(struct abac_request     * abac_req,
                         struct __cached_element * cached_element,
                         struct attribute_table  * attribute_table)
{
    struct hashmap_iter iter;

    hashmap_iter_init(&attribute_table->attribute_map, &iter);

    // start iterating the attribute table
    do {
        const struct attribute_term * attr_term;

        struct attribute_entry      * attr_entry = hashmap_iter_next(&iter);

        if (attr_entry == NULL) {
            break;
        }

        attr_term = attribute_store_find_uuid(abac_req->attribute_store, &attr_entry->attr_uuid);

        if (attr_term == NULL) {
            // TODO maybe report here?
            continue;
        }

        if (__register_attribute_fact(cached_element,
                                      &attr_entry->attr_uuid,
                                      attr_term->name,
                                      attr_entry->attr_val)) {
            log_error("could not push attribute pair (%s)\n", attr_term->name);
            return -1;
        }
    } while (1);

    return 0;
}

static int
__abac_request_insert_attributes(struct abac_request * abac_req)
{
    struct attribute_table * user_attr_table = abac_req->user_profile->attribute_table;

    // insert the user_profile attribute table
    if (__insert_attribute_table(abac_req, user_profile_element, user_attr_table)) {
        log_error("could not insert user_profile attribute table into database\n");
        goto out_err;
    }

    // insert the metadata attribute table
    if (abac_req->metadata) {
        struct attribute_table * obj_attr_table = metadata_get_attribute_table(abac_req->metadata);

        if (__insert_attribute_table(abac_req, abac_req->object_element, obj_attr_table)) {
            log_error("could not insert object attribute table into database\n");
            goto out_err;
        }
    }

    return 0;
out_err:
    return -1;
}

// --]] attributes


// --[[ rules

static int
__insert_policy_rule(struct abac_request * abac_req, struct policy_rule * rule)
{
    struct __cached_fact * cached_fact = NULL;

    cached_fact = cached_element_find_uuid_fact(policy_rules_element, &rule->rule_uuid);

    if (cached_fact == NULL) {
        cached_fact
            = cached_element_put_uuid_fact(policy_rules_element, &rule->rule_uuid, "", NULL);
        if (cached_fact == NULL) {
            log_error("could not cache policy rule\n");
            goto out_err;
        }

        cached_fact->is_rule = true;
    }

    if (cached_fact->is_inserted == false) {
        if (db_assert_policy_rule(rule)) {
            log_error("db_assert_policy_rule() FAILED\n");
            goto out_err;
        }

        cached_fact->is_inserted = true;
    }

    return 0;
out_err:
    return -1;
}

    static int
__abac_request_insert_rules(struct abac_request * abac_req)
{
    struct nexus_list_iterator * iter = NULL;

    if (abac_req->policy_store->rules_count == 0) {
        return NULL;
    }

    iter = list_iterator_new(&abac_req->policy_store->rules_list);

    do {
        struct policy_rule * rule = list_iterator_get(iter);

        if (__insert_policy_rule(abac_req, rule)) {
            list_iterator_free(iter);
            log_error("__insert_policy_rule() FAILED\n");
            return -1;
        }

        list_iterator_next(iter);
    } while(list_iterator_is_valid(iter));

    list_iterator_free(iter);

    return 0;
}

// --]] rules


static struct abac_request *
__create_abac_request(struct nexus_metadata * metadata, perm_type_t perm_type)
{
    struct abac_request * access_req = nexus_malloc(sizeof(struct abac_request));

    access_req->metadata  = metadata;
    access_req->perm_type = perm_type;

    access_req->policy_store = abac_acquire_policy_store(NEXUS_FREAD);
    if (access_req->policy_store == NULL) {
        log_error("could not acquire policy_store\n");
        return NULL;
    }

    access_req->attribute_store = abac_acquire_attribute_store(NEXUS_FREAD);
    if (access_req->attribute_store == NULL) {
        abac_release_policy_store();
        log_error("could not acquire attribute_store\n");
        return NULL;
    }

    access_req->user_profile = abac_acquire_current_user_profile(NEXUS_FREAD);
    if (access_req->user_profile == NULL) {
        abac_release_policy_store();
        abac_release_attribute_store();
        return NULL;
    }

    if (access_req->metadata) {
        access_req->object_element = __object_element_upsert(&access_req->metadata->uuid);
        if (access_req->object_element == NULL) {
            log_error("__upsert_element() for metadata failed FAILED\n");
            goto out_err;
        }
    }

    return access_req;
out_err:
    __destroy_abac_request(access_req);

    return NULL;
}

static void
__destroy_abac_request(struct abac_request * abac_req)
{
    abac_release_policy_store();
    abac_release_attribute_store();
    abac_release_current_user_profile();

    nexus_free(abac_req);
}

static int
__init_user_attributes()
{
    struct abac_request * abac_req = NULL;

    if (nexus_enclave_is_current_user_owner()) {
        return 0;
    }

    abac_req = __create_abac_request(NULL, PERM_READ);
    if (abac_req == NULL) {
        log_error("__create_abac_request() FAILED\n");
        return -1;
    }

    if (db_assert_cached_element_type(user_profile_element, USER_ATTRIBUTE_TYPE)) {
        log_error("db_assert_cached_element_type() FAILED\n");
        return -1;
    }

    if (__abac_request_insert_attributes(abac_req)) {
        log_error("__abac_request_insert_attributes() FAILED\n");
        __destroy_abac_request(abac_req);
        return -1;
    }

    if (__abac_request_insert_sysfuncs(abac_req)) {
        log_error("__abac_request_insert_sysfuncs() FAILED\n");
        __destroy_abac_request(abac_req);
        return -1;
    }

    __destroy_abac_request(abac_req);

    return 0;
}

static struct __cached_element *
__object_element_upsert(struct nexus_uuid * uuid)
{
    struct __cached_element * cached_element = nexus_lru_get(object_elements_map, uuid);

    if (cached_element) {
        return cached_element;
    }

    cached_element = cached_element_new(uuid);

    if (!nexus_lru_put(object_elements_map, &cached_element->uuid, cached_element)) {
        cached_element_free(cached_element);
        log_error("nexus_lru_put() FAILED\n");
        return NULL;
    }

    if (db_assert_cached_element_type(cached_element, OBJECT_ATTRIBUTE_TYPE)) {
        log_error("db_assert_cached_element_type() FAILED\n");
        return -1;
    }

    return cached_element;
}

static void
__object_element_freer(uintptr_t element, uintptr_t key)
{
    struct __cached_element * cached_element = element;

    if (cached_element->attr_type) {
        db_retract_cached_element_type(cached_element);
    }

    cached_element_free(element);
}

int
bouncer_init()
{
    my_database = datalog_engine_create();

    if (my_database == NULL) {
        log_error("could not create a new datalog engine\n");
        return -1;
    }

    // create the necessary structures
    {
        object_elements_map
            = nexus_lru_create(16, __uuid_hasher, __uuid_equals, __object_element_freer);

        policy_rules_element = cached_element_new(abac_policy_store_uuid());

        user_profile_element = cached_element_new(&global_user_struct->user_uuid);
    }

    if (__init_user_attributes()) {
        log_error("__init_user_attributes() FAILED\n");
        goto out_err;
    }

    return 0;
out_err:
    nexus_lru_destroy(object_elements_map);

    return -1;
}

void
bouncer_destroy()
{
    if (my_database) {
        datalog_engine_destroy(my_database);
    }

    if (object_elements_map) {
        nexus_lru_destroy(object_elements_map);
    }

    if (policy_rules_element) {
        cached_element_free(policy_rules_element);
    }

    if (user_profile_element) {
        cached_element_free(user_profile_element);
    }
}

bool
bouncer_access_check(struct nexus_metadata * metadata, perm_type_t perm_type)
{
    struct abac_request * abac_req = __create_abac_request(metadata, perm_type);

    if (abac_req == NULL) {
        log_error("__create_abac_request() FAILED\n");
        return -1;
    }

    if (__abac_request_insert_attributes(abac_req)) {
        log_error("__abac_request_insert_attributes() FAILED\n");
        goto out_err;
    }

    if (__abac_request_insert_rules(abac_req)) {
        log_error("__abac_request_insert_rules() FAILED\n");
        goto out_err;
    }

    // query the database
    {
        char * permission_string = perm_type_to_string(perm_type);

        if (!datalog_engine_is_true(my_database,
                                    permission_string,
                                    user_profile_element->uuid_str,
                                    abac_req->object_element->uuid_str)) {
            log_error("datalog_engine returned false\n");
            nexus_free(permission_string);
            goto out_err;
        }

        log_error("datalog_engine returned true\n");

        nexus_free(permission_string);
    }

    __destroy_abac_request(abac_req);

    return false;
out_err:
    __destroy_abac_request(abac_req);

    return false;
}

