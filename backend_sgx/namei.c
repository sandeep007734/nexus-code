static struct nexus_dentry *
d_alloc(struct nexus_dentry * parent,
        struct nexus_uuid   * uuid,
        const char          * name,
        nexus_dirent_type_t   type)
{
    struct nexus_dentry * dentry = nexus_malloc(sizeof(struct nexus_dentry));

    INIT_LIST_HEAD(&dentry->children);

    dentry->metadata_type = (type == NEXUS_DIR ? NEXUS_DIRNODE : NEXUS_FILENODE);
    dentry->parent        = parent;
    dentry->name_len      = strnlen(name, NEXUS_NAME_MAX);
    dentry->name          = strndup(name, NEXUS_NAME_MAX);

    nexus_uuid_copy(uuid, &dentry->uuid);

    return dentry;
}

static void
d_free(struct nexus_dentry * dentry)
{
    nexus_free(dentry->name);
    nexus_free(dentry);
}

static struct nexus_dentry *
create_dentry(struct nexus_dentry * parent,
              struct nexus_uuid   * uuid,
              const char          * name,
              nexus_dirent_type_t   type)
{
    struct nexus_dentry * dentry = d_alloc(parent, uuid, name, type);

    list_add_tail(&dentry->siblings, &parent->children);

    return dentry;
}

static void
d_prune(struct nexus_dentry * dentry)
{
    while (!list_empty(&dentry->children)) {
        struct nexus_dentry * first_child = NULL;

        first_child = list_first_entry(&dentry->children, struct nexus_dentry, siblings);

        list_del(&first_child->siblings);

        d_prune(first_child);

        d_free(first_child);
    }

    if (dentry->metadata) {
        // TODO refactor into VFS call
        dentry->metadata->dentry = NULL;
    }
}

static struct nexus_dentry *
d_lookup(struct nexus_dentry * parent, const char * name)
{
    struct list_head * curr = NULL;

    size_t len = strlen(name);

    list_for_each(curr, &parent->children)
    {
        struct nexus_dentry * dentry = NULL;

        dentry = list_entry(curr, struct nexus_dentry, siblings);

        if ((dentry->name_len == len) && (memcmp(name, dentry->name, len) == 0)) {
            return dentry;
        }
    }

    return NULL;
}

void
dentry_delete(struct nexus_dentry * dentry)
{
    list_del(&dentry->siblings);
    d_prune(dentry);
    d_free(dentry);
}

void
dentry_delete_child(struct nexus_dentry * parent_dentry, const char * child_filename)
{
    struct nexus_dentry * dentry = NULL;

    dentry = d_lookup(parent_dentry, child_filename);

    if (dentry != NULL) {
        dentry_delete(dentry);
    }
}

int
revalidate_dentry(struct nexus_dentry * dentry, nexus_io_flags_t flags)
{

    if (dentry->metadata) {
        if (nexus_vfs_revalidate(dentry->metadata, flags)) {
            log_error("could not revalidate dentry\n");
            return -1;
        }

        return 0;
    }

    // dentry->metadata = NULL
    dentry->metadata = nexus_vfs_load(&dentry->uuid, dentry->metadata_type, flags);

    if (dentry->metadata == NULL) {
        log_error("could not load metadata\n");
        return -1;
    }

    // otherwise, add dirnode to metadata list
    dentry->metadata->dentry = dentry;

    return 0;
}

struct nexus_dentry *
dentry_lookup(struct nexus_dentry * root_dentry, char * path)
{
    struct nexus_dentry * dentry  = NULL;

    struct path_builder builder;


    path_builder_init(&builder);

    if (path == NULL) {
        log_error("path cannot be null\n");
        return NULL;
    }

    if (path[0] == '\0') {
        dentry = root_dentry;
    } else {
        dentry = walk_path(root_dentry, path, &builder);
    }

    path_builder_free(&builder);

    return dentry;
}