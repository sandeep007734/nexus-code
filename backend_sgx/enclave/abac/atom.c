#include "abac_internal.h"
#include "atom.h"
#include "boolean_operators.h"

#include <libnexus_trusted/nexus_str.h>


struct __policy_atom_buf {
    atom_type_t             atom_type;
    pred_type_t             pred_type;

    struct nexus_uuid       attr_uuid; // 0s when not an attribute
    char                    predicate[SYSTEM_FUNC_MAX_LENGTH];

    uint16_t                arity;
    uint16_t                args_bufsize;

    uint8_t                 args_buffer[0];
} __attribute__((packed));


struct __atom_arg_buf {
    uint8_t                 arg_val[0];
} __attribute__((packed));


// --[[ atom_argument

static void
__atom_argument_free(struct atom_argument * atom_arg)
{
    if (atom_arg->abac_value) {
        nexus_free(atom_arg->abac_value);
    }

    nexus_free(atom_arg);
}

static size_t
__atom_argument_buf_size(struct atom_argument * atom_arg)
{
    return abac_value_bufsize(atom_arg->abac_value);
}

static struct atom_argument *
__atom_argument_from_buffer(uint8_t * buffer, size_t buflen, uint8_t ** output_ptr)
{
    struct atom_argument  * atom_arg = nexus_malloc(sizeof(struct atom_argument));
    struct __atom_arg_buf * arg_buf  = (struct __atom_arg_buf *)buffer;

    size_t bytes_left = buflen - sizeof(struct __atom_arg_buf);

    atom_arg->abac_value = abac_value_from_buf(&arg_buf->arg_val, bytes_left, output_ptr);

    if (atom_arg->abac_value == NULL) {
        log_error("abac_value_from_buf() FAILED\n");
        return NULL;
    }

    return atom_arg;
}

static uint8_t *
__atom_argument_to_buffer(struct atom_argument * atom_arg, uint8_t * buffer, size_t buflen)
{
    struct __atom_arg_buf * arg_buf    = (struct __atom_arg_buf *)buffer;

    size_t                  total_size = __atom_argument_buf_size(atom_arg);

    if (total_size > buflen) {
        log_error("the buffer is too small. total_size=%zu, buflen=%zu\n", total_size, buflen);
        return NULL;
    }

    // write the buffer
    size_t bytes_left = buflen - sizeof(struct __atom_arg_buf);

    if (abac_value_to_buf(atom_arg->abac_value, arg_buf->arg_val, bytes_left) == NULL) {
        log_error("abac_value_to_buf() FAILED\n");
        return NULL;
    }

    return (buffer + total_size);
}

static int
__atom_argument_to_datalog(struct atom_argument * atom_arg,
                           bool                   as_rule,
                           rapidstring          * string_builder)
{
    return abac_value_as_datalog(atom_arg->abac_value, string_builder, as_rule);
}

char *
atom_argument_string_val(const struct atom_argument * atom_arg)
{
    return abac_value_stringify(atom_arg->abac_value);
}

// atom_argument ]]--


struct policy_atom *
policy_atom_new()
{
    struct policy_atom * atom = nexus_malloc(sizeof(struct policy_atom));

    nexus_list_init(&atom->args_list);
    nexus_list_set_deallocator(&atom->args_list, __atom_argument_free);

    return atom;
}

struct policy_atom *
policy_atom_new_from_predicate(char * predicate)
{
    struct policy_atom * atom = policy_atom_new();
    policy_atom_set_predicate(atom, predicate);

    return atom;
}

void
policy_atom_free(struct policy_atom * atom)
{
    nexus_list_destroy(&atom->args_list);
    nexus_free(atom);
}

size_t
policy_atom_buf_size(struct policy_atom * atom)
{
    return sizeof(struct __policy_atom_buf) + atom->args_bufsize;
}

static int
__stringify_boolean_atom(struct policy_atom * atom, bool as_rule, rapidstring * string_builder)
{
    struct atom_argument * atom_arg = NULL;
    int index = 0;
    const char * datalog_str = boolean_operator_to_datalog_str(atom->predicate);

    if (datalog_str == NULL) {
        log_error("boolean_operator_to_datalog_str() returned NULL\n");
        return -1;
    }

    rs_cat(string_builder, datalog_str);
    rs_cat(string_builder, "(");

restart:
    atom_arg = nexus_list_get(&atom->args_list, index);

    if (__atom_argument_to_datalog(atom_arg, as_rule, string_builder)) {
        log_error("__atom_argument_to_datalog() FAILED\n");
        return -1;
    }

    index += 1;
    if (index < 2) {
        rs_cat(string_builder, ", ");
        goto restart;
    }

    rs_cat(string_builder, ")");

    return 0;
}

static int
__stringify_regular_atom(struct policy_atom * atom, bool as_rule, rapidstring * string_builder)
{
    rs_cat(string_builder, atom->predicate);
    rs_cat(string_builder, "(");

    if (atom->atom_type == ATOM_TYPE_USER) {
        rs_cat(string_builder, "U");
    } else if (atom->atom_type == ATOM_TYPE_OBJECT) {
        rs_cat(string_builder, "O");
    } else {
        log_error("atom type should be object or user\n");
        goto out_err;
    }

    // we only have 1 argument
    if (atom->arity) {
        struct atom_argument * atom_arg = nexus_list_get(&atom->args_list, 0);
        if (atom_arg == NULL) {
            log_error("could not get atom_arg in atom\n");
            goto out_err;
        }

        rs_cat_n(string_builder, ", ", 2);

        if (__atom_argument_to_datalog(atom_arg, as_rule, string_builder)) {
            log_error("__atom_argument_to_datalog() FAILED\n");
            return -1;
        }
    }

    rs_cat(string_builder, ")");

    return 0;
out_err:
    return -1;
}

int
__policy_atom_to_str(struct policy_atom * atom, bool as_rule, rapidstring * string_builder)
{
    if (atom->pred_type == PREDICATE_ATTR || atom->pred_type == PREDICATE_FUNC) {
        if (__stringify_regular_atom(atom, as_rule, string_builder)) {
            return -1;
        }
    } else if (atom->pred_type == PREDICATE_BOOL) {
        if (__stringify_boolean_atom(atom, as_rule, string_builder)) {
            return -1;
        }
    } else {
        log_error("unknown predicate type\n");
        return -1;
    }

    return 0;
}

char *
policy_atom_to_str(struct policy_atom * atom, bool as_rule)
{
    char *      result_string = NULL;
    rapidstring string_builder;

    // "10" is arbirtrary, it's my estimate for spacing and commas
    rs_init_w_cap(&string_builder, policy_atom_buf_size(atom) + 10);

    if (__policy_atom_to_str(atom, as_rule, &string_builder)) {
        log_error("__policy_atom_to_str() FAILED\n");
        rs_free(&string_builder);
        return NULL;
    }

    result_string = strndup(rs_data_c(&string_builder), rs_len(&string_builder));
    rs_free(&string_builder);

    return result_string;
}

uint8_t *
policy_atom_to_buf(struct policy_atom * atom, uint8_t * buffer, size_t buflen)
{
    struct __policy_atom_buf * atom_buffer   = (struct __policy_atom_buf *)buffer;

    size_t                     atom_buf_size = policy_atom_buf_size(atom);

    uint8_t                  * output_ptr    = NULL;
    size_t                     output_len    = 0;


    if (atom_buf_size > buflen) {
        log_error("buffer is too small to store atom\n");
        return NULL;
    }

    // serialize the header
    {
        atom_buffer->atom_type    = atom->atom_type;
        atom_buffer->pred_type    = atom->pred_type;

        atom_buffer->arity        = atom->arity;
        atom_buffer->args_bufsize = atom->args_bufsize;

        nexus_uuid_copy(&atom->attr_uuid, &atom_buffer->attr_uuid);
        memcpy(atom_buffer->predicate, atom->predicate, SYSTEM_FUNC_MAX_LENGTH);
    }

    // initialize the output_ptr and output_len
    output_ptr = (buffer + sizeof(struct __policy_atom_buf));
    output_len = (buflen - sizeof(struct __policy_atom_buf));

    {
        struct nexus_list_iterator * iter = list_iterator_new(&atom->args_list);

        while (list_iterator_is_valid(iter)) {
            struct atom_argument * atom_arg = list_iterator_get(iter);

            uint8_t * next_ptr = __atom_argument_to_buffer(atom_arg, output_ptr, output_len);

            if (next_ptr == NULL) {
                log_error("nexus_string_to_buf() FAILED\n");
                list_iterator_free(iter);
                return NULL;
            }

            output_len -= (uintptr_t)(next_ptr - output_ptr);
            output_ptr = next_ptr;

            list_iterator_next(iter);
        }

        list_iterator_free(iter);
    }

    // make sure that the output_ptr is at the endof the buffer
    if (output_ptr != (buffer + atom_buf_size)) {
        log_error("the output_ptr is not at the end of the buffer\n");
        return NULL;
    }

    return (buffer + atom_buf_size);
}

struct policy_atom *
policy_atom_from_buf(uint8_t * buffer, size_t buflen, uint8_t ** output_ptr)
{
    struct __policy_atom_buf * tmp_atom_buf = (struct __policy_atom_buf *)buffer;

    struct policy_atom       * new_policy_atom = NULL;

    size_t total_size = sizeof(struct __policy_atom_buf) + tmp_atom_buf->args_bufsize;

    if (buflen < total_size) {
        log_error("the atom buffer is too small. buflen=%zu, atom_bufsize=%zu\n",
                  buflen,
                  total_size);
        return NULL;
    }

    new_policy_atom = policy_atom_new();

    // parse the header
    {
        new_policy_atom->atom_type = tmp_atom_buf->atom_type;
        new_policy_atom->pred_type = tmp_atom_buf->pred_type;

        new_policy_atom->arity        = tmp_atom_buf->arity;
        new_policy_atom->args_bufsize = tmp_atom_buf->args_bufsize;

        nexus_uuid_copy(&tmp_atom_buf->attr_uuid, &new_policy_atom->attr_uuid);
        memcpy(new_policy_atom->predicate, tmp_atom_buf->predicate, SYSTEM_FUNC_MAX_LENGTH);
    }

    // set the output_ptr to the start of the buffer
    uint8_t * next_ptr = *output_ptr = tmp_atom_buf->args_buffer;
    buflen -= sizeof(struct __policy_atom_buf);

    // parse the args buffer
    for (size_t i = 0; i < tmp_atom_buf->arity; i++) {
        struct atom_argument * atom_arg = __atom_argument_from_buffer(next_ptr, buflen, output_ptr);

        if (atom_arg == NULL) {
            log_error("__atom_argument_from_buffer() FAILED\n");
            goto out_err;
        }

        nexus_list_append(&new_policy_atom->args_list, atom_arg);

        buflen -= (*output_ptr - next_ptr);
        next_ptr = *output_ptr;
    }

    if (*output_ptr != (buffer + total_size)) {
        log_error("output_ptr is not at the end of read buffer\n");
        goto out_err;
    }

    return new_policy_atom;
out_err:
    policy_atom_free(new_policy_atom);
    return NULL;
}

int
policy_atom_push_arg(struct policy_atom * atom, struct abac_value * abac_value)
{
    struct atom_argument * atom_arg = nexus_malloc(sizeof(struct atom_argument));

    atom_arg->abac_value = abac_value;

    atom->arity        += 1;
    atom->args_bufsize += __atom_argument_buf_size(atom_arg);

    nexus_list_append(&atom->args_list, atom_arg);

    return 0;
}

const struct atom_argument *
policy_atom_get_arg(struct policy_atom * atom, size_t index)
{
    return nexus_list_get(&atom->args_list, index);
}

void
policy_atom_set_uuid(struct policy_atom * atom, struct nexus_uuid * uuid)
{
    nexus_uuid_copy(uuid, &atom->attr_uuid);
}

void
policy_atom_set_predicate(struct policy_atom * atom, char * predicate_str)
{
    memset(&atom->predicate, 0, SYSTEM_FUNC_MAX_LENGTH);
    strncpy(&atom->predicate, predicate_str, SYSTEM_FUNC_MAX_LENGTH);

    switch (atom->predicate[0]) {
    case '>':
    case '<':
    case '=':
        atom->pred_type = PREDICATE_BOOL;
        break;
    case '@':
        atom->pred_type = PREDICATE_FUNC;
        break;
    default:
        atom->pred_type = PREDICATE_ATTR;
    }
}

static bool
__check_attribute(char * attribute_name, atom_type_t atom_type, struct nexus_uuid * uuid_optional)
{
    const struct attribute_term * term             = NULL;
    struct attribute_store      * global_attrstore = abac_acquire_attribute_store(NEXUS_FREAD);

    if (global_attrstore == NULL) {
        log_error("could not acquire attribute_store\n");
        return false;
    }

    term = attribute_store_find_name(global_attrstore, attribute_name);

    if (term == NULL) {
        log_error("could not find attribute (%s) in store\n", attribute_name);
        goto out_err;
    }

    if (atom_type == ATOM_TYPE_USER) {
        if (term->type != USER_ATTRIBUTE_TYPE) {
            log_error("the atom_type is `user`, but attribute_type is NOT\n");
            goto out_err;
        }
    } else if (atom_type == ATOM_TYPE_OBJECT) {
        if (term->type != OBJECT_ATTRIBUTE_TYPE) {
            log_error("the atom_type is `object`, but attribute_type is NOT\n");
            goto out_err;
        }
    } else {
        log_error("unknown atom type\n");
        goto out_err;
    }

    if (uuid_optional) {
        nexus_uuid_copy(&term->uuid, uuid_optional);
    }

    abac_release_attribute_store();

    return true;
out_err:
    abac_release_attribute_store();

    return false;
}

// FIXME: this function should have an optional argument that checks for the uuid.
// this will allow policies to validate their atoms even after an attribute has been aliased.
static bool
__validate_abac_attribute_atom(struct policy_atom * atom)
{
    return __check_attribute(atom->predicate, atom->atom_type, &atom->attr_uuid);
}

static bool
__check_system_function(char * function_name, atom_type_t atom_type)
{
    if (atom_type == ATOM_TYPE_OBJECT) {
        if (!system_function_exists(function_name, OBJECT_FUNCTION)) {
            log_error("could not find object function (%s)\n", function_name);
            return false;
        }

        return true;
    } else if (atom_type == ATOM_TYPE_USER) {
        if (!system_function_exists(function_name, USER_FUNCTION)) {
            log_error("could not find user function (%s)\n", function_name);
            return false;
        }

        return true;
    }

    log_error("unknown atom_type\n");
    return false;
}

static bool
__validate_system_function_atom(struct policy_atom * atom)
{
    return __check_system_function(atom->predicate, atom->atom_type);
}

static bool
__validate_boolean_operator_atom(struct policy_atom * atom)
{
    if (atom->arity != 2) {
        log_error("boolean operator atom must have arity of 2\n");
        return false;
    }

    for (int index = 0; index < 2; index++) {
        struct atom_argument * atom_arg = nexus_list_get(&atom->args_list, index);

        if (atom_arg->abac_value->type != ABAC_VALUE_IDENTIFIER) {
            continue;
        }

        // get the string, and validate the attribute/sys_function
        char * string_val = __abac_value_get_rawptr(atom_arg->abac_value);

        if (strnlen(string_val, _ABAC_VALUE_MAXLEN) < 2) {
            log_error("symbol size is too small\n");
            return false;
        }

        atom_type_t atom_type = atom_type_from_char(string_val[0]);
        if (atom_type == ATOM_TYPE_NONE) {
            log_error("invalid symbole(%s), neither user or object\n", string_val);
            return false;
        }

        if (string_val[2] == '@') {
            if (!__check_system_function(&string_val[2], atom_type)) {
                log_error("__check_system_function() FAILED\n");
                return false;
            }
        } else {
            if (!__check_attribute(&string_val[2], atom_type, NULL)) {
                log_error("__check_attribute() FAILED\n");
                return false;
            }
        }
    }

    return true;
}

bool
policy_atom_is_valid(struct policy_atom * atom)
{
    if (atom->pred_type == PREDICATE_FUNC) {
        return __validate_system_function_atom(atom);
    } else if (atom->pred_type == PREDICATE_ATTR) {
        return __validate_abac_attribute_atom(atom);
    } else if (atom->pred_type == PREDICATE_BOOL) {
        return __validate_boolean_operator_atom(atom);
    }

    log_error("unknown atom type\n");

    return false;
}

int
policy_atom_add_to_engine(struct policy_atom * atom, dl_db_t db)
{
    struct nexus_list_iterator * iter = NULL;


    if (atom->pred_type == PREDICATE_BOOL) {
        log_error("boolean atoms are not supported\n");
        return -1;
    }

    if (dl_pushliteral(db)) {
        log_error("dl_pushliteral() for atom FAILED\n");
        return -1;
    }

    // push the predicate
    if (dl_pushstring(db, atom->predicate)) {
        log_error("dl_pushstring('%s')\n", atom->predicate);
        return -1;
    }

    if (dl_addpred(db)) {
        log_error("dl_addpred() of atom's predicate FAILED\n");
        return -1;
    }

    // push "u|o"
    {
        const char * user_or_object_term = NULL;

        switch (atom->atom_type) {
        case ATOM_TYPE_OBJECT:
            user_or_object_term = "O";
            break;
        case ATOM_TYPE_USER:
            user_or_object_term = "U";
            break;
        default:
            log_error("unknown atom type\n");
            return -1;
        }

        if (dl_pushstring(db, user_or_object_term)) {
            log_error("dl_pushstring() user_or_object_term\n");
            return -1;
        }

        if (dl_addvar(db)) {
            log_error("dl_addvar() FAILED\n");
            return -1;
        }
    }

    // push the nuumber of string argument
    iter = list_iterator_new(&atom->args_list);

    {
        const struct atom_argument * atom_arg = list_iterator_get(iter);

        if (atom_arg == NULL) {
            log_error("iteratorr returned NULL\n");
            goto out_err;
        }

        char * string_val = atom_argument_string_val(atom_arg);

        if (string_val == NULL) {
            log_error("atom_argument_string_val() FAILED\n");
            goto out_err;
        }

        if (dl_pushstring(db, string_val)) {
            log_error("dl_pushstring(`%s`) FAILED\n", string_val);
            nexus_free(string_val);
            goto out_err;
        }

        nexus_free(string_val);

        switch (atom_arg->abac_value->type) {
        case ABAC_VALUE_IDENTIFIER:
            if (dl_addvar(db)) {
                log_error("could not add atom's variable\n");
                goto out_err;
            }
            break;
        case ABAC_VALUE_STRING:
        case ABAC_VALUE_NUMBER:
            if (dl_addconst(db)) {
                log_error("could not add atom's constant\n");
                goto out_err;
            }
            break;
        default:
            log_error("abac value unknown\n");
            goto out_err;
        }

        list_iterator_next(iter);
    } while (list_iterator_is_valid(iter));

    if (dl_makeliteral(db)) {
        log_error("dl_makeliteral() FAILED\n");
        goto out_err;
    }

    list_iterator_free(iter);
    return 0;

out_err:
    list_iterator_free(iter);
    return -1;
}

