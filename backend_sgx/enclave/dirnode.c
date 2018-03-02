#include "enclave_internal.h"

// This is how the dirnode will be serialized onto a buffer
struct __dirnode_hdr {
    struct nexus_uuid my_uuid;
    struct nexus_uuid root_uuid;

    uint32_t symlink_count;
    uint32_t symlink_buflen;

    uint32_t dir_entry_count; // number of files & subdirs
    uint32_t dir_entry_buflen;
} __attribute__((packed));



/* file and subdirectories in a folder */

struct dir_entry_s {
    uint16_t            total_len;
    nexus_dirent_type_t type;
    struct nexus_uuid   uuid;
    uint16_t            name_len;
    char                name[0];
} __attribute__((packed));


struct symlink_entry_s {
    uint8_t           total_len;
    struct nexus_uuid uuid;
    uint16_t          target_len;
    char              target_path[0];
} __attribute__((packed));



static struct nexus_list_iterator *
__find_by_name(struct nexus_dirnode * dirnode, const char * fname);

/**
 * Would be called as directory entries are freed
 * @param el
 */
static void
__free_dir_entry(void * el);

static void
__free_symlink_entry(void * el);

static void
dirnode_init(struct nexus_dirnode * dirnode);



// --------------------------- dir entry stuff -------------------------------

static struct dir_entry_s *
__new_dir_entry(struct nexus_uuid * entry_uuid, nexus_dirent_type_t type, char * filename)
{
    struct dir_entry_s * new_dir_entry = NULL;

    size_t name_len  = 0;
    size_t total_len = 0;

    name_len  = strlen(filename);
    total_len = sizeof(struct dir_entry_s) + name_len + 1;

    new_dir_entry = nexus_malloc(total_len);

    new_dir_entry->total_len = total_len;
    new_dir_entry->name_len  = name_len;
    new_dir_entry->type      = type;

    memcpy(new_dir_entry->name, filename, name_len);

    nexus_uuid_copy(entry_uuid, &new_dir_entry->uuid);

    return new_dir_entry;
}

static void
__free_dir_entry(void * el)
{
    struct dir_entry_s * dir_entry = (struct dir_entry_s *)el;

    nexus_free(dir_entry);
}

static inline struct dir_entry_s *
__clone_dir_entry(struct dir_entry_s * src_dir_entry)
{
    struct dir_entry_s * new_dir_entry = NULL;

    new_dir_entry = nexus_malloc(src_dir_entry->total_len);

    memcpy(new_dir_entry, src_dir_entry, src_dir_entry->total_len);

    return new_dir_entry;
}

static uint8_t *
__parse_dir_entry(struct dir_entry_s ** dir_entry, uint8_t * in_buffer)
{
    struct dir_entry_s * new_dir_entry = NULL;
    struct dir_entry_s * tmp_dir_entry = NULL;

    tmp_dir_entry = (struct dir_entry_s *)in_buffer;

    new_dir_entry = __clone_dir_entry(tmp_dir_entry);

    *dir_entry = new_dir_entry;

    return (in_buffer + new_dir_entry->total_len);
}

static uint8_t *
__serialize_dir_entry(struct dir_entry_s * dir_entry, uint8_t * out_buffer)
{
    memcpy(out_buffer, dir_entry, dir_entry->total_len);

    return (out_buffer + dir_entry->total_len);
}



// ------------------------- symlink stuff --------------------------

static void
__free_symlink_entry(void * el)
{
    struct symlink_entry_s * symlink = (struct symlink_entry_s *)el;

    nexus_free(symlink);
}

static uint8_t *
__parse_symlink_entry(struct symlink_entry_s ** symlink_entry, uint8_t * in_buffer)
{
    struct symlink_entry_s * new_symlink_entry = NULL;
    struct symlink_entry_s * tmp_symlink_entry = NULL;

    tmp_symlink_entry = (struct symlink_entry_s *)in_buffer;


    new_symlink_entry = nexus_malloc(tmp_symlink_entry->total_len);

    memcpy(new_symlink_entry, tmp_symlink_entry, tmp_symlink_entry->total_len);

    *symlink_entry = new_symlink_entry;

    return (in_buffer + tmp_symlink_entry->total_len);
}

static uint8_t *
__serialize_symlink_entry(struct symlink_entry_s * symlink_entry, uint8_t * out_buffer)
{
    memcpy(out_buffer, symlink_entry, symlink_entry->total_len);

    return (out_buffer + symlink_entry->total_len);
}

static size_t
__get_total_size(struct nexus_dirnode * dirnode)
{
    return sizeof(struct __dirnode_hdr)
           + dirnode->symlink_buflen
           + dirnode->dir_entry_buflen
           + nexus_acl_size(&dirnode->dir_acl);
}


static uint8_t *
__parse_dirnode_header(struct nexus_dirnode * dirnode,
                       uint8_t              * buffer,
                       size_t                 buflen,
                       size_t               * bytes_left)
{
    struct __dirnode_hdr * header = NULL;

    if (buflen < sizeof(struct __dirnode_hdr)) {
        log_error("buffer is too small for a dirnode\n");
        return NULL;
    }

    header = (struct __dirnode_hdr *)buffer;

    nexus_uuid_copy(&header->my_uuid, &dirnode->my_uuid);
    nexus_uuid_copy(&header->root_uuid, &dirnode->root_uuid);

    dirnode->symlink_count  = header->symlink_count;
    dirnode->symlink_buflen = header->symlink_buflen;

    dirnode->dir_entry_count  = header->dir_entry_count;
    dirnode->dir_entry_buflen = header->dir_entry_buflen;

    *bytes_left = (buflen - sizeof(struct __dirnode_hdr));

    return buffer + sizeof(struct __dirnode_hdr);
}

struct nexus_dirnode *
dirnode_from_buffer(uint8_t * buffer, size_t buflen)
{
    struct nexus_dirnode  * dirnode    = NULL;

    size_t bytes_left = 0;

    uint8_t * input_ptr = NULL;


    dirnode = nexus_malloc(sizeof(struct nexus_dirnode));

    input_ptr = __parse_dirnode_header(dirnode, buffer, buflen, &bytes_left);

    if (input_ptr == NULL) {
        nexus_free(dirnode);

        log_error("__parse_dirnode_header FAILED\n");
        return NULL;
    }

    dirnode_init(dirnode);

    {
        int ret = __nexus_acl_from_buffer(&dirnode->dir_acl, input_ptr, bytes_left);

        if (ret != 0) {
            dirnode_free(dirnode);

            log_error("__nexus_acl_from_buffer() FAILED\n");
            return NULL;
        }

        input_ptr += nexus_acl_size(&dirnode->dir_acl);
    }

    for (size_t i = 0; i < dirnode->symlink_count; i++) {
        struct symlink_entry_s * new_symlink_entry = NULL;

        input_ptr = __parse_symlink_entry(&new_symlink_entry, input_ptr);

        nexus_list_append(&dirnode->symlink_list, new_symlink_entry);
    }

    for (size_t i = 0; i < dirnode->dir_entry_count; i++) {
        struct dir_entry_s * new_dir_entry   = NULL;

        input_ptr = __parse_dir_entry(&new_dir_entry, input_ptr);

        nexus_list_append(&dirnode->dir_entry_list, new_dir_entry);
    }

    return dirnode;
}

int
dirnode_compare(struct nexus_dirnode * src_dirnode, struct nexus_dirnode * dst_dirnode)
{
    return nexus_uuid_compare(&src_dirnode->my_uuid, &dst_dirnode->my_uuid);
}

struct nexus_dirnode *
dirnode_load(struct nexus_uuid * uuid)
{
    struct nexus_dirnode * dirnode = NULL;

    struct nexus_crypto_buf * crypto_buffer = NULL;

    uint8_t * buffer = NULL;
    size_t    buflen = 0;


    crypto_buffer = nexus_crypto_buf_create(uuid);

    if (crypto_buffer == NULL) {
        log_error("metadata_read FAILED\n");
        return NULL;
    }

    buffer = nexus_crypto_buf_get(crypto_buffer, &buflen, NULL);

    if (buffer == NULL) {
        nexus_crypto_buf_free(crypto_buffer);
        log_error("nexus_crypto_buf_get() FAILED\n");
        return NULL;
    }


    dirnode = dirnode_from_buffer(buffer, buflen);

    nexus_crypto_buf_free(crypto_buffer);

    if (dirnode == NULL) {
        log_error("__parse_dirnode FAILED\n");
        return NULL;
    }

    return dirnode;
}

static uint8_t *
__serialize_dirnode_header(struct nexus_dirnode * dirnode, uint8_t * buffer)
{
    struct __dirnode_hdr * header = (struct __dirnode_hdr *)buffer;

    nexus_uuid_copy(&dirnode->my_uuid, &header->my_uuid);
    nexus_uuid_copy(&dirnode->root_uuid, &header->root_uuid);

    header->symlink_count  = dirnode->symlink_count;
    header->symlink_buflen = dirnode->symlink_buflen;

    header->dir_entry_count  = dirnode->dir_entry_count;
    header->dir_entry_buflen = dirnode->dir_entry_buflen;

    return buffer + sizeof(struct __dirnode_hdr);
}

static int
dirnode_serialize(struct nexus_dirnode * dirnode, uint8_t * buffer)
{
    uint8_t * output_ptr = NULL;

    output_ptr = __serialize_dirnode_header(dirnode, buffer);

    if (output_ptr == NULL) {
        log_error("serializing dirnode header FAILED\n");
        return -1;
    }

    // serialize the directory ACLS
    {
        int ret = nexus_acl_to_buffer(&dirnode->dir_acl, output_ptr);

        if (ret != 0) {
            log_error("nexus_acl_to_buffer() FAILED\n");
            return -1;
        }

        output_ptr += nexus_acl_size(&dirnode->dir_acl);
    }

    // writeout the symlinks
    {
        struct nexus_list_iterator * iter = NULL;

        iter = list_iterator_new(&dirnode->symlink_list);

        while (list_iterator_is_valid(iter)) {
            struct symlink_entry_s * symlink_entry = list_iterator_get(iter);

            output_ptr = __serialize_symlink_entry(symlink_entry, output_ptr);

            list_iterator_next(iter);
        }

        list_iterator_free(iter);
    }

    // iterate through the dir entries and write to the buffer
    {
        struct nexus_list_iterator * iter = NULL;

        iter = list_iterator_new(&dirnode->dir_entry_list);

        while (list_iterator_is_valid(iter)) {
            struct dir_entry_s * dir_entry = list_iterator_get(iter);

            output_ptr = __serialize_dir_entry(dir_entry, output_ptr);

            list_iterator_next(iter);
        }

        list_iterator_free(iter);
    }

    return 0;
}


static void
dirnode_init(struct nexus_dirnode * dirnode)
{
    nexus_list_init(&dirnode->dir_entry_list);
    nexus_list_set_deallocator(&dirnode->dir_entry_list, __free_dir_entry);

    nexus_list_init(&dirnode->symlink_list);
    nexus_list_set_deallocator(&dirnode->symlink_list, __free_symlink_entry);

    nexus_acl_init(&dirnode->dir_acl);
}

struct nexus_dirnode *
dirnode_create(struct nexus_uuid * root_uuid, struct nexus_uuid * my_uuid)
{
    struct nexus_dirnode * dirnode = NULL;

    dirnode = nexus_malloc(sizeof(struct nexus_dirnode));

    nexus_uuid_copy(root_uuid, &dirnode->root_uuid);
    nexus_uuid_copy(my_uuid, &dirnode->my_uuid);

    dirnode_init(dirnode);

    return dirnode;
}

int
dirnode_store(struct nexus_dirnode * dirnode, struct nexus_mac * mac)
{
    struct nexus_crypto_buf * crypto_buffer = NULL;

    size_t serialized_buflen = 0;

    int ret = -1;


    serialized_buflen = __get_total_size(dirnode);

    crypto_buffer = nexus_crypto_buf_new(serialized_buflen, &dirnode->my_uuid);
    if (!crypto_buffer) {
        goto out;
    }

    // write to the buffer
    {
        uint8_t * output_buffer = NULL;

        size_t    buffer_size   = 0;


        output_buffer = nexus_crypto_buf_get(crypto_buffer, &buffer_size, NULL);
        if (output_buffer == NULL) {
            log_error("could not get the crypto_buffer buffer\n");
            goto out;
        }

        ret = dirnode_serialize(dirnode, output_buffer);
        if (ret != 0) {
            log_error("dirnode_serialize() FAILED\n");
            goto out;
        }

        ret = nexus_crypto_buf_put(crypto_buffer, mac);
        if (ret != 0) {
            log_error("nexus_crypto_buf_put FAILED\n");
            goto out;
        }
    }

    ret = nexus_crypto_buf_flush(crypto_buffer);
    if (ret) {
        log_error("metadata_write FAILED\n");
        goto out;
    }

    ret = 0;
out:
    if (crypto_buffer) {
        nexus_crypto_buf_free(crypto_buffer);
    }

    return ret;
}

void
dirnode_free(struct nexus_dirnode * dirnode)
{
    nexus_list_destroy(&dirnode->dir_entry_list);

    nexus_acl_free(&dirnode->dir_acl);

    nexus_free(dirnode);
}

int
dirnode_add(struct nexus_dirnode * dirnode,
            char                 * filename,
            nexus_dirent_type_t    type,
            struct nexus_uuid    * entry_uuid)
{
    struct dir_entry_s * new_dir_entry = NULL;

    // check for existing entry.
    // XXX: typical filesystems perform a lookup to check if the file exists before
    // adding the file. Consisder caching dirnode lookups
    {
        struct nexus_list_iterator * iter = NULL;

        iter = __find_by_name(dirnode, filename);

        if (iter) {
            // the file exists
            list_iterator_free(iter);
            return -1;
        }
    }

    new_dir_entry = __new_dir_entry(entry_uuid, type, filename);

    nexus_list_append(&dirnode->dir_entry_list, new_dir_entry);

    dirnode->dir_entry_count  += 1;
    dirnode->dir_entry_buflen += new_dir_entry->total_len;

    return 0;
}

int
dirnode_add_link(struct nexus_dirnode * dirnode,
                 char                 * link_name,
                 char                 * target_path,
                 struct nexus_uuid    * entry_uuid)
{
    struct symlink_entry_s * symlink_entry = NULL;

    size_t target_len = 0;
    size_t total_len  = 0;


    if (dirnode_add(dirnode, link_name, NEXUS_LNK, entry_uuid) != 0) {
        log_error("dirnode_add() FAILED\n");
        return -1;
    }

    target_len = strnlen(target_path, NEXUS_PATH_MAX);
    total_len  = target_len + sizeof(struct symlink_entry_s) + 1;

    symlink_entry = nexus_malloc(total_len);

    symlink_entry->total_len  = total_len;
    symlink_entry->target_len = target_len;

    nexus_uuid_copy(entry_uuid, &symlink_entry->uuid);
    memcpy(symlink_entry->target_path, target_path, target_len);

    nexus_list_append(&dirnode->symlink_list, symlink_entry);

    dirnode->symlink_count  += 1;
    dirnode->symlink_buflen += total_len;

    return 0;
}

static struct nexus_list_iterator *
__find_by_uuid(struct nexus_dirnode * dirnode, struct nexus_uuid * uuid)
{
    struct dir_entry_s * dir_entry = NULL;

    struct nexus_list_iterator * iter = NULL;

    iter = list_iterator_new(&dirnode->dir_entry_list);

    while (list_iterator_is_valid(iter)) {
        dir_entry = list_iterator_get(iter);

        if (nexus_uuid_compare(&dir_entry->uuid, uuid) == 0) {
            return iter;
        }

        list_iterator_next(iter);
    }

    list_iterator_free(iter);

    return NULL;
}

int
dirnode_find_by_uuid(struct nexus_dirnode * dirnode,
                     struct nexus_uuid    * uuid,
                     nexus_dirent_type_t  * p_type,
                     const char          ** p_fname,
                     size_t               * p_fname_len)
{
    struct dir_entry_s * dir_entry = NULL;

    struct nexus_list_iterator * iter = NULL;

    iter = __find_by_uuid(dirnode, uuid);

    if (iter == NULL) {
        return -1;
    }


    dir_entry = list_iterator_get(iter);

    *p_type      = dir_entry->type;
    *p_fname     = dir_entry->name;
    *p_fname_len = dir_entry->name_len;

    list_iterator_free(iter);

    return 0;
}

static struct nexus_list_iterator *
__find_by_name(struct nexus_dirnode * dirnode, const char * fname)
{
    struct dir_entry_s * dir_entry = NULL;

    struct nexus_list_iterator * iter = NULL;

    iter = list_iterator_new(&dirnode->dir_entry_list);

    while (list_iterator_is_valid(iter)) {
        dir_entry = list_iterator_get(iter);

        if (strncmp(dir_entry->name, fname, dir_entry->name_len) == 0) {
            return iter;
        }

        list_iterator_next(iter);
    }

    list_iterator_free(iter);

    return NULL;
}

int
dirnode_find_by_name(struct nexus_dirnode * dirnode,
                     char                 * filename,
                     nexus_dirent_type_t  * type,
                     struct nexus_uuid    * entry_uuid)
{
    struct dir_entry_s * dir_entry = NULL;

    struct nexus_list_iterator * iter = NULL;


    iter = __find_by_name(dirnode, filename);

    if (iter == NULL) {
        return -1;
    }

    dir_entry = list_iterator_get(iter);

    nexus_uuid_copy(&dir_entry->uuid, entry_uuid);
    *type = dir_entry->type;

    list_iterator_free(iter);

    return 0;
}

static struct nexus_list_iterator *
__find_symlink(struct nexus_dirnode * dirnode, struct nexus_uuid * uuid)
{
    struct symlink_entry_s     * symlink_entry = NULL;

    struct nexus_list_iterator * iter          = NULL;

    iter = list_iterator_new(&dirnode->symlink_list);

    while (list_iterator_is_valid(iter)) {
        symlink_entry = list_iterator_get(iter);

        if (nexus_uuid_compare(&symlink_entry->uuid, uuid) == 0) {
            return iter;
        }

        list_iterator_next(iter);
    }

    list_iterator_free(iter);

    return NULL;
}

static int
__remove_symlink(struct nexus_dirnode * dirnode, struct nexus_uuid * entry_uuid, char ** target_path_dest)
{
    struct symlink_entry_s * symlink_entry = NULL;

    struct nexus_list_iterator * iter = NULL;

    iter = __find_symlink(dirnode, entry_uuid);

    if (iter == NULL) {
        return -1;
    }

    symlink_entry = list_iterator_get(iter);

    if (target_path_dest) {
        *target_path_dest = strndup(symlink_entry->target_path, NEXUS_PATH_MAX);
    }

    dirnode->symlink_count  -= 1;
    dirnode->symlink_buflen -= symlink_entry->total_len;

    list_iterator_del(iter);
    list_iterator_free(iter);

    return 0;
}

char *
dirnode_get_link(struct nexus_dirnode * dirnode, struct nexus_uuid * entry_uuid)
{
    struct symlink_entry_s * symlink_entry = NULL;

    struct nexus_list_iterator * iter = NULL;

    iter = __find_symlink(dirnode, entry_uuid);

    if (iter == NULL) {
        return NULL;
    }

    symlink_entry = list_iterator_get(iter);

    list_iterator_free(iter);

    return symlink_entry->target_path;
}

static inline int
__dirnode_remove(struct nexus_dirnode * dirnode,
                 char                 * filename,
                 nexus_dirent_type_t  * type,
                 struct nexus_uuid    * entry_uuid)
{
    struct dir_entry_s * dir_entry = NULL;

    struct nexus_list_iterator * iter = NULL;


    iter = __find_by_name(dirnode, filename);

    if (iter == NULL) {
        return -1;
    }

    dir_entry = list_iterator_get(iter);

    // adjust the dirnode
    dirnode->dir_entry_count  -= 1;
    dirnode->dir_entry_buflen -= dir_entry->total_len;

    *type = dir_entry->type;

    nexus_uuid_copy(&dir_entry->uuid, entry_uuid);

    // remove from the list
    list_iterator_del(iter);
    list_iterator_free(iter);

    return 0;
}

int
dirnode_remove(struct nexus_dirnode * dirnode,
               char                 * filename,
               nexus_dirent_type_t  * type,
               struct nexus_uuid    * entry_uuid,
               char                ** symlink_target_path)
{
    int ret = -1;

    ret = __dirnode_remove(dirnode, filename, type, entry_uuid);

    if (*type == NEXUS_LNK) {
        __remove_symlink(dirnode, entry_uuid, symlink_target_path);
    }

    return ret;
}
