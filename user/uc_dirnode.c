#include "uc_encode.h"
#include "uc_sgx.h"
#include "uc_uspace.h"
#include "uc_vfs.h"

#include "third/log.h"

static int
serialize_lockbox(uc_dirnode_t * dn, uint8_t * buffer);

static int
parse_lockbox(uc_dirnode_t * dn, uint8_t * buffer);

static int
serialize_dirbox(uc_dirnode_t * dn, uint8_t * buffer);

static int
parse_dirbox(uc_dirnode_t * dn, uint8_t * buffer);

static int
serialize_journal(uc_dirnode_t * dn, uint8_t * buffer);

static int
parse_journal(uc_dirnode_t * dn, uint8_t * buffer);

uc_dirnode_t *
dirnode_new2(const shadow_t * id, const uc_dirnode_t * parent)
{
    uc_dirnode_t * dn = (uc_dirnode_t *)malloc(sizeof(uc_dirnode_t));
    if (dn == NULL) {
        return NULL;
    }

    memset(&dn->header, 0, sizeof(dnode_header_t));
    if (id) {
        memcpy(&dn->header.uuid, id, sizeof(shadow_t));
    } else {
        uuid_generate_time_safe((uint8_t *)&dn->header.uuid);
    }

    TAILQ_INIT(&dn->dirbox);
    SIMPLEQ_INIT(&dn->lockbox);
    TAILQ_INIT(&dn->journal);

    dn->dnode_path = NULL;
    dn->dentry = NULL;

    if (parent) {
        memcpy(&dn->header.parent, &parent->header.uuid, sizeof(shadow_t));
        memcpy(&dn->header.root, &parent->header.root, sizeof(shadow_t));
    }

    return dn;
}

uc_dirnode_t *
dirnode_new_root(const shadow_t * id)
{
    uc_dirnode_t * dn;
    if ((dn = dirnode_new2(id, NULL)) == NULL) {
        return NULL;
    }

    /* make sure we put stuff that makes it lok like root */
    memcpy(&dn->header.parent, id, sizeof(shadow_t));
    memcpy(&dn->header.root, id, sizeof(shadow_t));

    return dn;
}

uc_dirnode_t *
dirnode_new_alias(const shadow_t * id)
{
    return dirnode_new2(id, NULL);
}

uc_dirnode_t *
dirnode_new()
{
    return dirnode_new_alias(NULL);
}

static inline void
free_list_entry(dnode_list_entry_t * list_entry)
{
    if (list_entry->dnode_data.target) {
        free(list_entry->dnode_data.target);
    }

    free(list_entry);
}

void
dirnode_free(uc_dirnode_t * dirnode)
{
    dnode_list_head_t * dir_head = &dirnode->dirbox;
    acl_list_head_t * acl_head = &dirnode->lockbox;
    journal_list_head_t * journal_head = &dirnode->journal;

    dnode_list_entry_t * dnode_data;
    acl_list_entry_t * acl_entry;
    journal_list_entry_t * journal_entry;

    /* clear the entries in the entries */
    while ((dnode_data = TAILQ_FIRST(dir_head))) {
        TAILQ_REMOVE(dir_head, dnode_data, next_entry);
        free_list_entry(dnode_data);
    }

    /* clear lockbox entries */
    while ((acl_entry = SIMPLEQ_FIRST(acl_head))) {
        SIMPLEQ_REMOVE_HEAD(acl_head, next_entry);
        free(acl_entry);
    }

    /* clear the journal entries */
    while ((journal_entry = TAILQ_FIRST(journal_head))) {
        TAILQ_REMOVE(journal_head, journal_entry, next_entry);
        free(journal_entry);
    }

    if (dirnode->dnode_path) {
        sdsfree(dirnode->dnode_path);
    }

    free(dirnode);
}

uc_dirnode_t *
dirnode_from_file(const sds filepath)
{
    uc_dirnode_t * dn = NULL;
    dnode_header_t * header;
    dnode_list_head_t * dirbox;
    acl_list_head_t * lockbox;
    journal_list_head_t * journal;
    uint8_t *buffer = NULL, *offset_ptr = NULL;
    FILE * fd;
    size_t nbytes, body_len;
    int error = -1;

    fd = fopen(filepath, "rb");
    if (fd == NULL) {
        log_error("opening '%s' failed", filepath);
        return NULL;
    }

    /* instantiate our objects */
    if ((dn = calloc(sizeof(uc_dirnode_t), 1)) == NULL) {
        log_fatal("allocation failed");
        fclose(fd);
        return NULL;
    }

    header = &dn->header;
    dirbox = &dn->dirbox;
    lockbox = &dn->lockbox;
    journal = &dn->journal;

    TAILQ_INIT(dirbox);
    SIMPLEQ_INIT(lockbox);
    TAILQ_INIT(journal);

    /* read the header from the file */
    nbytes = fread(header, sizeof(dnode_header_t), 1, fd);
    if (!nbytes) {
        log_error("reading header: %s (nbytes=%zu, exp=%lu)", filepath, nbytes,
                  sizeof(dnode_header_t));
        goto out;
    }

    /* lets try to read the body of the dirnode */
    // TODO maybe check when body_len is ridiculous?
    body_len = header->dirbox_len + header->lockbox_len + header->journal_len;
    if (!body_len) {
        goto done;
    }

    if ((buffer = (uint8_t *)malloc(body_len)) == NULL) {
        log_fatal("allocation for dnode failed");
        goto out;
    }

    if ((nbytes = fread(buffer, 1, body_len, fd)) != body_len) {
        log_error("reading metadata: expected=%zu, actual=%zu", body_len,
                  nbytes);
        goto out;
    }

#ifdef UCAFS_SGX
    /* decrypt the content with enclave */
    ecall_crypto_dirnode(global_eid, &error, header, buffer, UC_DECRYPT);
    if (error) {
        log_error("enclave dirnode decryption failed");
        goto out;
    }
#endif

    /* parse the body */
    offset_ptr = buffer;
    if (header->dirbox_len && parse_dirbox(dn, offset_ptr)) {
        log_error("parsing dirbox failed: %s", filepath);
        goto out;
    }

    offset_ptr += header->dirbox_len;
    if (header->lockbox_len && parse_lockbox(dn, offset_ptr)) {
        log_error("parsing lockbox failed: %s", filepath);
        goto out;
    }

    offset_ptr += header->lockbox_len;
    if (header->journal_len && parse_journal(dn, offset_ptr)) {
        log_error("parsing journal failed: %s", filepath);
        goto out;
    }

done:
    dn->dnode_path = sdsdup(filepath);
    error = 0;

out:
    if (error) {
        dirnode_free(dn);
        dn = NULL;
    }

    if (buffer) {
        free(buffer);
    }

    fclose(fd);
    return dn;
}

bool
dirnode_write(uc_dirnode_t * dn, const char * fpath)
{
    bool ret = false;
    int error;
    uint8_t *buffer = NULL, *offset_ptr = NULL;
    FILE * fd;
    size_t proto_len, total_len;

    total_len = dn->header.dirbox_len + dn->header.lockbox_len
        + dn->header.journal_len;

    fd = fopen(fpath, "wb");
    if (fd == NULL) {
        log_error("file not found: %s", fpath);
        return false;
    }

    if ((buffer = (uint8_t *)malloc(total_len)) == NULL) {
        log_fatal("allocation error (%s)", __func__);
        goto out;
    }

    offset_ptr = buffer;
    if (serialize_dirbox(dn, offset_ptr)) {
        log_error("serialization failed");
        goto out;
    }

    /* now serialize the the access control information */
    offset_ptr += dn->header.journal_len;
    if (serialize_lockbox(dn, offset_ptr)) {
        log_error("serializing dirnode ACL failed (%s)", fpath);
        goto out;
    }

    offset_ptr += dn->header.lockbox_len;
    if (serialize_journal(dn, offset_ptr)) {
        log_error("serializing dirnode journal failed (%s)", fpath);
        goto out;
    }

#ifdef UCAFS_SGX
    ecall_crypto_dirnode(global_eid, &error, &dn->header, buffer, UC_ENCRYPT);
    if (error) {
        log_error("enclave encryption failed (%s)", __func__);
        goto out;
    }
#endif

    fwrite(&dn->header, sizeof(dnode_header_t), 1, fd);
    fwrite(buffer, total_len, 1, fd);

    ret = true;
out:
    fclose(fd);

    if (buffer) {
        free(buffer);
    }

    return ret;
}

bool
dirnode_flush(uc_dirnode_t * dn)
{
    bool ret = dn->dnode_path ? dirnode_write(dn, dn->dnode_path) : false;
    if (ret && dn->mcache) {
        metadata_update_entry(dn->mcache);
    }

    return ret;
}

bool
dirnode_fsync(uc_dirnode_t * dn)
{
    return dirnode_flush(dn);
}

void
dirnode_lockbox_clear(uc_dirnode_t * dn)
{
    acl_list_entry_t * acl_entry;
    acl_list_head_t * acl_list = &dn->lockbox;

    while (!SIMPLEQ_EMPTY(acl_list)) {
        acl_entry = SIMPLEQ_FIRST(acl_list);
        SIMPLEQ_REMOVE_HEAD(acl_list, next_entry);
        free(acl_entry);
    }

    SIMPLEQ_INIT(acl_list);
    dn->header.lockbox_len = 0;
    dn->header.lockbox_count = 0;
}

int
dirnode_lockbox_add(uc_dirnode_t * dn, const char * name, acl_rights_t rights)
{
    int len = strlen(name), total = sizeof(acl_list_entry_t) + len + 1;
    acl_data_t * acl_data;
    acl_list_entry_t * acl_entry = (acl_list_entry_t *)malloc(total);
    if (acl_entry == NULL) {
        return -1;
    }

    acl_data = &acl_entry->acl_data;
    acl_data->rights = rights;
    acl_data->len = len;
    memcpy(acl_data->name, name, len);
    acl_data->name[len] = '\0';

    SIMPLEQ_INSERT_TAIL(&dn->lockbox, acl_entry, next_entry);
    dn->header.lockbox_len += sizeof(acl_data_t) + len + 1;
    dn->header.lockbox_count++;

    return 0;
}

int
dirnode_checkacl(uc_dirnode_t * dn, acl_rights_t rights)
{
    int ret = -1;

#ifdef UCAFS_SGX
    ecall_check_rights(global_eid, &ret, &dn->header, &dn->lockbox, rights);
    if (ret) {
        goto out;
    }
#endif

    ret = 0;
out:
    return ret;
}

// TODO
shadow_t *
dirnode_add_alias(uc_dirnode_t * dn,
                  const char * name,
                  ucafs_entry_type type,
                  const shadow_t * p_encoded_name,
                  const link_info_t * link_info)
{
    int ret = -1, len, tlen, rec_len;
    shadow_t * shdw_name = NULL;
    dnode_list_entry_t * list_entry = NULL;
    dnode_data_t * de;

    if (type == UC_ANY) {
        return NULL;
    }

    len = strlen(name) + 1, rec_len = sizeof(dnode_dir_payload_t) + len,
    tlen = sizeof(dnode_list_entry_t) + len;

    list_entry = (dnode_list_entry_t *)calloc(1, tlen);
    if (list_entry == NULL) {
        log_fatal("allocation error");
        return NULL;
    }

    de = &list_entry->dnode_data;
    de->type = (uint8_t)type;
    if (p_encoded_name) {
        memcpy(&de->shadow_name, p_encoded_name, sizeof(shadow_t));
    } else {
        uuid_generate_time_safe(de->shadow_name.bin);
    }

    de->name_len = len;
    memcpy(de->real_name, name, len - 1);

    /* set the link data */
    if (link_info) {
        len = link_info->total_len - sizeof(link_info_t);

        if ((de->target = malloc(len)) == NULL) {
            log_fatal("allocation error");
            goto out;
        }

        memcpy(de->target, link_info->target_link, len);

        de->link_len = len;
        rec_len += de->link_len;
    }

    de->rec_len = rec_len;
    dn->header.dirbox_len += rec_len;
    dn->header.dirbox_count++;

    TAILQ_INSERT_TAIL(&dn->dirbox, list_entry, next_entry);

    if ((shdw_name = (shadow_t *)malloc(sizeof(shadow_t))) == NULL) {
        log_fatal("allocation error");
        return NULL;
    }

    memcpy(shdw_name, &de->shadow_name, sizeof(shadow_t));

    ret = 0;
out:
    // TODO implement free list_entry
    if (ret) {
        if (shdw_name) {
            free(shdw_name);
            shdw_name = NULL;
        }
    }

    return shdw_name;
}

shadow_t *
dirnode_add(uc_dirnode_t * dn, const char * name, ucafs_entry_type type)
{
    return dirnode_add_alias(dn, name, type, NULL, NULL);
}

shadow_t *
dirnode_add_link(uc_dirnode_t * dn,
                 const char * link_name,
                 const link_info_t * link_info)
{
    return dirnode_add_alias(dn, link_name, UC_LINK, NULL, link_info);
}

static inline dnode_list_entry_t *
iterate_by_realname(uc_dirnode_t * dn,
                    const char * realname,
                    ucafs_entry_type * p_type,
                    link_info_t ** pp_link_info)
{
    dnode_list_entry_t * list_entry;
    dnode_data_t * de;
    link_info_t * link_info;

    int len = strlen(realname) + 1, len1;

    TAILQ_FOREACH(list_entry, &dn->dirbox, next_entry)
    {
        de = &list_entry->dnode_data;

        if (len == de->name_len && memcmp(realname, de->real_name, len) == 0) {
            /* XXX this needs to be deprecated. */
            if (pp_link_info && de->link_len) {
                len1 = de->link_len + sizeof(link_info_t);

                if ((link_info = (link_info_t *)malloc(len1)) == NULL) {
                    log_fatal("allocation error");
                    return NULL;
                }

                link_info->total_len = len1;
                link_info->type
                    = (de->type == UC_LINK) ? UC_SOFTLINK : UC_HARDLINK;
                memcpy(link_info->target_link, de->target, de->link_len);
                *pp_link_info = link_info;
            }

            *p_type = de->type;
            return list_entry;
        }
    }

    return NULL;
}

shadow_t *
dirnode_rm(uc_dirnode_t * dn,
           const char * realname,
           ucafs_entry_type type,
           ucafs_entry_type * p_type,
           link_info_t ** pp_link_info)
{
    shadow_t * result = NULL;
    dnode_list_entry_t * list_entry;
    dnode_data_t * de;
    int len1;

    list_entry = iterate_by_realname(dn, realname, p_type, pp_link_info);
    if (list_entry == NULL) {
        return NULL;
    }

    de = &list_entry->dnode_data;

    if ((result = (shadow_t *)malloc(sizeof(shadow_t))) == NULL) {
        log_fatal("allocation error");
        return NULL;
    }

    memcpy(result, &de->shadow_name, sizeof(shadow_t));

    /* TODO: delete from the dentry and metadata */
    dn->header.dirbox_count--;
    dn->header.dirbox_len -= de->rec_len;

    TAILQ_REMOVE(&dn->dirbox, list_entry, next_entry);
    free_list_entry(list_entry);

    return result;
out:
    if (result) {
        free(result);
    }

    return NULL;
}

static const shadow_t *
__dirnode_raw2enc(uc_dirnode_t * dn,
                  const char * realname,
                  ucafs_entry_type type,
                  ucafs_entry_type * p_type,
                  const link_info_t ** pp_link_info)
{
    dnode_list_entry_t * list_entry = iterate_by_realname(
        dn, realname, p_type, (link_info_t **)pp_link_info);

    return list_entry ? &list_entry->dnode_data.shadow_name : NULL;
}

const shadow_t *
dirnode_raw2enc(uc_dirnode_t * dn,
                const char * realname,
                ucafs_entry_type type,
                ucafs_entry_type * p_type)
{
    return __dirnode_raw2enc(dn, realname, type, p_type, NULL);
}

const shadow_t *
dirnode_traverse(uc_dirnode_t * dn,
                 const char * realname,
                 ucafs_entry_type type,
                 ucafs_entry_type * p_type,
                 const link_info_t ** pp_link_info)
{
    return __dirnode_raw2enc(dn, realname, type, p_type, pp_link_info);
}

const char *
dirnode_enc2raw(uc_dirnode_t * dn,
                const shadow_t * encoded_name,
                ucafs_entry_type type,
                ucafs_entry_type * p_type)
{
    int ret;
    dnode_list_entry_t * list_entry;
    dnode_data_t * de;

    TAILQ_FOREACH(list_entry, &dn->dirbox, next_entry)
    {
        de = &list_entry->dnode_data;

        ret = memcmp(&de->shadow_name, encoded_name, sizeof(shadow_t));
        if (ret == 0) {
            *p_type = de->type;
            return de->real_name;
        }
    }

    return NULL;
}

// TODO fix locking
int
dirnode_rename(uc_dirnode_t * dn,
               const char * oldname,
               const char * newname,
               ucafs_entry_type type,
               ucafs_entry_type * p_type,
               shadow_t ** ptr_shadow1_bin,
               shadow_t ** ptr_shadow2_bin,
               link_info_t ** pp_link_info1,
               link_info_t ** pp_link_info2)
{
    shadow_t * shadow2_bin;
    ucafs_entry_type atype, atype1;

    *ptr_shadow2_bin = NULL;

    *ptr_shadow1_bin = dirnode_rm(dn, oldname, type, &atype, pp_link_info1);
    if (*ptr_shadow1_bin) {
        // it is necessary to return the codename of the existing entry
        // otherwise, we get a lingering file in the AFS server
        //
        // Pass the UNKOWN flag to ensure any copy of the existing file is
        // erased
        shadow2_bin = dirnode_rm(dn, newname, UC_ANY, &atype1, pp_link_info2);
        if (shadow2_bin == NULL) {
            shadow2_bin = dirnode_add(dn, newname, atype);
        } else {
            // in case the source was a link, its information gets carried over
            shadow2_bin = dirnode_add_alias(dn, newname, atype, shadow2_bin,
                                            *pp_link_info1);
        }

        *p_type = atype;
        *ptr_shadow2_bin = shadow2_bin;
        return shadow2_bin == NULL ? -1 : 0;
    }
    return -1;
}

// will generate the serialized version of the dirnode
static int
serialize_dirbox(uc_dirnode_t * dn, uint8_t * buffer)
{
    int len;
    dnode_list_head_t * list_head = &dn->dirbox;
    dnode_list_entry_t * list_entry;
    dnode_data_t * de;

    TAILQ_FOREACH(list_entry, list_head, next_entry)
    {
        de = &list_entry->dnode_data;

        /* lets write the static data */
        len = de->rec_len - de->link_len;
        memcpy(buffer, &de->static_data, len);
        buffer += len;

        /* write out the link info */
        if ((len = de->link_len)) {
            memcpy(buffer, de->target, len);
            buffer += len;
        }
    }

    return 0;
}

static int
parse_dirbox(uc_dirnode_t * dn, uint8_t * buffer)
{
    int len, ret = -1;
    size_t sz;
    dnode_list_head_t * list_head = &dn->dirbox;
    dnode_list_entry_t * list_entry;
    dnode_data_t * de;
    dnode_dir_payload_t * payload = (dnode_dir_payload_t *)buffer;

    for (size_t i = 0; i < dn->header.dirbox_count; i++) {
        /* instantiate the list entry */
        sz = sizeof(dnode_list_entry_t)
            + (len = (payload->rec_len - payload->link_len));

        if ((list_entry = (dnode_list_entry_t *)malloc(sz)) == NULL) {
            log_fatal("allocation error");
            goto out;
        }

        de = &list_entry->dnode_data;
        // copy the static data
        memcpy(&de->static_data, buffer, len);
        buffer += len;

        if ((len = payload->link_len)) {
            if ((de->target = malloc(len)) == NULL) {
                log_fatal("allocation error");
                goto out;
            }

            memcpy(de->target, buffer, len);
            buffer += len;
        } else {
            de->target = NULL;
        }

        TAILQ_INSERT_TAIL(list_head, list_entry, next_entry);

        // move to the next entry
        payload = (dnode_dir_payload_t *)buffer;
    }

    ret = 0;
out:
    // TODO on error, clear the created entries
    return ret;
}

static int
serialize_lockbox(uc_dirnode_t * dn, uint8_t * buffer)
{
    int len;
    acl_list_entry_t * acl_entry;
    acl_data_t * acl_data;
    uint8_t * buf = buffer;
    acl_list_head_t * acl_list = &dn->lockbox;

    // iterate through the list of all the entries
    SIMPLEQ_FOREACH(acl_entry, acl_list, next_entry)
    {
        acl_data = &acl_entry->acl_data;
        len = sizeof(acl_data_t) + acl_data->len;

        memcpy(buf, acl_data, len);

        buf += len;
    }

    return 0;
}

static int
parse_lockbox(uc_dirnode_t * dn, uint8_t * buffer)
{
    int ret = -1, len, total_len, i, count = dn->header.lockbox_count;
    acl_list_head_t * acl_list = &dn->lockbox;
    acl_data_t *acl_data, *acl_buffer;
    acl_list_entry_t * acl_entry;
    acl_buffer = (acl_data_t *)buffer;

    for (i = 0; i < count; i++) {
        len = acl_buffer->len, total_len = sizeof(acl_data_t) + len;

        acl_entry
            = (acl_list_entry_t *)malloc(sizeof(acl_list_entry_t) + len + 1);
        if (acl_entry == NULL) {
            log_fatal("allocation error");
            goto out;
        }

        /* copy the data from the buffer */
        acl_data = &acl_entry->acl_data;
        memcpy(acl_data, acl_buffer, total_len);

        SIMPLEQ_INSERT_TAIL(acl_list, acl_entry, next_entry);

        acl_buffer = (acl_data_t *)(((caddr_t)acl_buffer) + total_len);
    }

    ret = 0;
out:
    return ret;
}

bool
dirnode_has_garbage(uc_dirnode_t * dirnode)
{
    return dirnode->header.garbage_count > 0;
}

const shadow_t *
dirnode_peek_garbage(uc_dirnode_t * dirnode)
{
    // XXX please improve this
    journal_list_entry_t * list_entry;
    journal_list_head_t * list_head = &dirnode->journal;

    TAILQ_FOREACH(list_entry, list_head, next_entry)
    {
        if (list_entry->jrnl_data.op == DELETE_FILE) {
            return &list_entry->jrnl_data.shdw;
        }
    }

    return NULL;
}

int
dirnode_search_journal(uc_dirnode_t * dn,
                       const shadow_t * shdw,
                       journal_op_t * op)
{
    journal_data_t * je;
    journal_list_entry_t * list_entry;
    journal_list_head_t * list_head = &dn->journal;

    // iterate to find an existing entry
    TAILQ_FOREACH(list_entry, list_head, next_entry) {
        je = &list_entry->jrnl_data;

        if (memcmp(shdw, &je->shdw, sizeof(shadow_t)) == 0) {
            *op = je->op;
            return 0;
        }
    }

    return -1;
}

int
dirnode_add_to_journal(uc_dirnode_t * dn,
                       const shadow_t * shdw,
                       journal_op_t op)
{
    journal_data_t * je;
    journal_list_entry_t * list_entry;
    journal_list_head_t * list_head = &dn->journal;

    // iterate to find an existing entry
    TAILQ_FOREACH(list_entry, list_head, next_entry)
    {
        je = &list_entry->jrnl_data;

        if (memcmp(shdw, &je->shdw, sizeof(shadow_t)) == 0) {
            if (je->op == CREATE_FILE && op == DELETE_FILE) {
                // then we've to delete the entry
                TAILQ_REMOVE(list_head, list_entry, next_entry);
                dn->header.journal_count--;

                free(list_entry);
            }

            // if we're here, we're either adding a duplicate entry (same op)
            // or removing a previous CREATE because we are adding a delete.
            // Either way, this becomes a noop
            return 0;
        }
    }

    list_entry = (journal_list_entry_t *)malloc(sizeof(journal_list_entry_t));
    if (list_entry == NULL) {
        return -1;
    }

    je = &list_entry->jrnl_data;
    memcpy(&je->shdw, shdw, sizeof(shadow_t));
    je->op = op;

    // add it to the dirnode
    TAILQ_INSERT_TAIL(&dn->journal, list_entry, next_entry);
    dn->header.journal_count++;
    dn->header.journal_len += sizeof(journal_data_t);
    dn->header.garbage_count += (op == DELETE_FILE);

    return 0;
}

void
dirnode_rm_from_journal(uc_dirnode_t * dn, const shadow_t * shdw)
{
    journal_list_entry_t * list_entry;
    journal_list_head_t * list_head = &dn->journal;
    journal_data_t * je;

    TAILQ_FOREACH(list_entry, list_head, next_entry)
    {
        je = &list_entry->jrnl_data;
        if (memcmp(&je->shdw, shdw, sizeof(shadow_t)) == 0) {
            // bingo
            dn->header.garbage_count -= (je->op == DELETE_FILE);
            dn->header.journal_count--;

            TAILQ_REMOVE(list_head, list_entry, next_entry);
            free(list_entry);
        }
    }
}

static int
serialize_journal(uc_dirnode_t * dn, uint8_t * buffer)
{
    int len = sizeof(journal_data_t);
    journal_list_entry_t * list_entry;
    journal_list_head_t * list_head = &dn->journal;
    journal_data_t * je;

    TAILQ_FOREACH(list_entry, list_head, next_entry)
    {
        je = &list_entry->jrnl_data;
        memcpy(buffer, je, len);
        buffer += len;
    }

    return 0;
}

static int
parse_journal(uc_dirnode_t * dn, uint8_t * buffer)
{
    int ret = -1, len = sizeof(journal_data_t);
    journal_list_entry_t * list_entry;
    journal_list_head_t * list_head = &dn->journal;
    journal_data_t * je;

    for (size_t i = 0; i < dn->header.journal_count; i++) {
        list_entry
            = (journal_list_entry_t *)malloc(sizeof(journal_list_entry_t));
        if (list_entry == NULL) {
            log_fatal("allocation error");
            goto out;
        }

        memcpy(&list_entry->jrnl_data, buffer, len);
        buffer += len;

        TAILQ_INSERT_TAIL(list_head, list_entry, next_entry);
    }

    ret = 0;
out:
    // TODO on error, clear created entries
    return ret;
}

void
dirnode_set_parent(uc_dirnode_t * dirnode, const uc_dirnode_t * parent)
{
    memcpy(&dirnode->header.parent, &parent->header.uuid, sizeof(shadow_t));
}

const shadow_t *
dirnode_get_parent(uc_dirnode_t * dirnode)
{
    return &dirnode->header.parent;
}

struct metadata_entry *
dirnode_get_metadata(uc_dirnode_t * dn)
{
    return dn->mcache;
}

void
dirnode_set_metadata(uc_dirnode_t * dn, struct metadata_entry * entry)
{
    // XXX get the lock here?
    dn->mcache = entry;
}

void
dirnode_set_dentry(uc_dirnode_t * dirnode, struct uc_dentry * dentry)
{
    dirnode->dentry = dentry;
}

struct uc_dentry *
dirnode_get_dentry(uc_dirnode_t * dirnode)
{
    return dirnode->dentry;
}

void
dirnode_clear_dentry(uc_dirnode_t * dirnode)
{
    dirnode->dentry = NULL;
}

const sds
dirnode_get_fpath(uc_dirnode_t * dirnode)
{
    return dirnode->dnode_path;
}

const shadow_t *
dirnode_get_root(uc_dirnode_t * dirnode)
{
    return &dirnode->header.root;
}

bool
dirnode_equals(uc_dirnode_t * dn1, uc_dirnode_t * dn2)
{
    return memcmp(&dn1->header, &dn2->header, sizeof(dnode_header_t)) == 0;
}

