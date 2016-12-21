#include "ucafs_mod.h"

#undef ERROR
#define ERROR(fmt, args...) printk(KERN_ERR "ucafs_kern: " fmt, ##args)

static const char * afs_prefix = "/afs";
static const uint32_t afs_prefix_len = 4;

static char * watch_dirs[] = {UCAFS_PATH_KERN "/" UC_AFS_WATCH};
static const int watch_dir_len[] = {sizeof(watch_dirs[0]) - 1};

inline caddr_t
READPTR_LOCK(void)
{
    if (mutex_lock_interruptible(&dev->mut)) {
        ERROR("locking mutex failed\n");
        return 0;
    }

    /* clear the message at that pointer */
    memset(dev->outb, 0, sizeof(ucrpc_msg_t));
    return (caddr_t)((char *)dev->outb + sizeof(ucrpc_msg_t));
}

inline void
READPTR_UNLOCK(void)
{
    mutex_unlock(&dev->mut);
}

// hold READPTR_LOCK()
inline size_t
READPTR_BUFLEN(void)
{
    return (dev->buffersize - dev->avail_read - sizeof(ucrpc_msg_t));
}

inline ucafs_entry_type
dentry_type(const struct dentry * dentry)
{
    if (d_is_file(dentry)) {
        return UC_FILE;
    } else if (d_is_dir(dentry)) {
        return UC_DIR;
    } else if (d_is_symlink(dentry)) {
        return UC_LINK;
    }

    return UC_ANY;
}

inline ucafs_entry_type
vnode_type(const struct vcache * vnode)
{
    if (vnode == NULL) {
        return UC_ANY;
    }

    switch (vType(vnode)) {
    case VREG:
        return UC_FILE;
    case VDIR:
        return UC_DIR;
    case VLNK:
        return UC_LINK;
    }

    return UC_ANY;
}

bool
startsWith(const char * pre, const char * str)
{
    size_t lenpre = strlen(pre), lenstr = strlen(str);
    return lenstr < lenpre ? 0 : strncmp(pre, str, lenpre) == 0;
}

int
ucafs_dentry_path(const struct dentry * dentry, char ** dest)
{
    int len, i, total_len;
    char *path, *curr_dir, *result;
    char buf[512];

    if (dentry == NULL) {
        return 1;
    }

    /* TODO cache the inode number
    printk(KERN_ERR "\npar=%p, dentry=%p, iname=%s d_name.len=%d
    dentry_name=%s",
           dentry->d_parent, dentry, dentry->d_iname, dentry->d_name.len,
           dentry->d_name.name); */
    path = dentry_path_raw((struct dentry *)dentry, buf, sizeof(buf));

    if (IS_ERR_OR_NULL(path)) {
        print_hex_dump(KERN_ERR, "", DUMP_PREFIX_ADDRESS, 32, 1, buf,
                       sizeof(buf), 1);
        return 1;
    }

    /*
    printk(KERN_ERR "path=%p\n", path);
    print_hex_dump(KERN_ERR, "", DUMP_PREFIX_ADDRESS, 32, 1, buf, sizeof(buf),
                   1); */

    for (i = 0; i < sizeof(watch_dirs) / sizeof(char *); i++) {
        curr_dir = watch_dirs[i];

        if (startsWith(curr_dir, path)) {
            // TODO maybe check the prefix on the name
            // we're good
            if (dest) {
                len = strlen(path);
                total_len = afs_prefix_len + len;
                result = kmalloc(total_len + 1, GFP_KERNEL);
                memcpy(result, afs_prefix, afs_prefix_len);
                memcpy(result + afs_prefix_len, path, len);
                result[total_len] = '\0';
                *dest = result;
            }
            return 0;
        }
    }

    return 1;
}

inline int
ucafs_vnode_path(const struct vcache * avc, char ** dest)
{
    if (avc == NULL || vnode_type(avc) == UC_ANY) {
        return -1;
    }

    return ucafs_dentry_path(d_find_alias(AFSTOV((struct vcache *)avc)), dest);
}

void
ucafs_kern_ping(void)
{
    static int num = 0;
    int err, code;
    XDR xdrs, *rsp = NULL;
    caddr_t payload;

    if ((payload = READPTR_LOCK()) == 0) {
        return;
    }

    /* create the XDR object */
    xdrmem_create(&xdrs, payload, READPTR_BUFLEN(), XDR_ENCODE);
    if (!xdr_int(&xdrs, &num)) {
        ERROR("xdr_int failed\n");
        READPTR_UNLOCK();
        goto out;
    }

    num++;

    /* send eveything */
    if (ucafs_mod_send(UCAFS_MSG_PING, &xdrs, &rsp, &code) || code) {
        num--;
        goto out;
    }

    err = 0;
out:
    if (rsp) {
        kfree(rsp);
    }
}

static int
__ucafs_parent_aname_req(uc_msg_type_t msg_type,
                         struct vcache * avc,
                         char * name,
                         ucafs_entry_type type,
                         char ** shadow_name)
{
    int ret = -1, code;
    XDR xdrs, *rsp = NULL;
    caddr_t payload;
    char * path;

    if (ucafs_vnode_path(avc, &path)) {
        return -1;
    }

    if ((payload = READPTR_LOCK()) == 0) {
        return -1;
    }

    xdrmem_create(&xdrs, payload, READPTR_BUFLEN(), XDR_ENCODE);
    if (!xdr_string(&xdrs, &path, UCAFS_PATH_MAX) ||
        !xdr_string(&xdrs, &name, UCAFS_FNAME_MAX) ||
        !xdr_int(&xdrs, (int *)&type)) {
        ERROR("xdr create failed\n");
        READPTR_UNLOCK();
        goto out;
    }

    if (ucafs_mod_send(msg_type, &xdrs, &rsp, &code) || code) {
        goto out;
    }

    /* read the response */
    if (!xdr_string(rsp, shadow_name, UCAFS_FNAME_MAX)) {
        ERROR("parsing shadow_name failed");
        goto out;
    }

    ret = 0;
out:
    kfree(path);

    if (rsp) {
        kfree(rsp);
    }

    return ret;
}

int
ucafs_kern_create(struct vcache * avc,
                  char * name,
                  ucafs_entry_type type,
                  char ** shadow_name)
{
    return __ucafs_parent_aname_req(UCAFS_MSG_CREATE, avc, name, type,
                                    shadow_name);
}

int
ucafs_kern_lookup(struct vcache * avc,
                  char * name,
                  ucafs_entry_type type,
                  char ** shadow_name)
{
    return __ucafs_parent_aname_req(UCAFS_MSG_LOOKUP, avc, name, type,
                                    shadow_name);
}

int
ucafs_kern_remove(struct vcache * avc,
                  char * name,
                  ucafs_entry_type type,
                  char ** shadow_name)
{
    return __ucafs_parent_aname_req(UCAFS_MSG_REMOVE, avc, name, type,
                                    shadow_name);
}

int
ucafs_kern_symlink(struct vcache * avc,
                   char * name,
                   ucafs_entry_type type,
                   char ** shadow_name)
{
    return __ucafs_parent_aname_req(UCAFS_MSG_SYMLINK, avc, name, type,
                                    shadow_name);
}

int
ucafs_kern_hardlink(struct vcache * avc,
                    char * name,
                    ucafs_entry_type type,
                    char ** shadow_name)
{
    return __ucafs_parent_aname_req(UCAFS_MSG_HARDLINK, avc, name, type,
                                    shadow_name);
}

int
ucafs_kern_filldir(char * parent_dir,
                   char * shadow_name,
                   ucafs_entry_type type,
                   char ** real_name)
{
    int err = -1, code;
    XDR xdrs, *rsp = NULL;
    caddr_t payload;

    if ((payload = READPTR_LOCK()) == 0) {
        return -1;
    }

    /* create the XDR object */
    xdrmem_create(&xdrs, payload, READPTR_BUFLEN(), XDR_ENCODE);
    if (!(xdr_string(&xdrs, &parent_dir, UCAFS_PATH_MAX)) ||
        !(xdr_string(&xdrs, &shadow_name, UCAFS_FNAME_MAX)) ||
        !xdr_int(&xdrs, (int *)&type)) {
        ERROR("xdr filldir failed\n");
        READPTR_UNLOCK();
        goto out;
    }

    /* send eveything */
    if (ucafs_mod_send(UCAFS_MSG_FILLDIR, &xdrs, &rsp, &code) || code) {
        goto out;
    }

    /* read the response */
    if (!xdr_string(rsp, real_name, UCAFS_FNAME_MAX)) {
        ERROR("parsing shadow_name failed");
        goto out;
    }

    err = 0;
out:
    if (rsp) {
        kfree(rsp);
    }

    return err;
}
