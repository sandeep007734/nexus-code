#include "ucafs_mod.h"

#undef ERROR
#define ERROR(fmt, args...) printk(KERN_ERR "ucafs_mod: " fmt, ##args)

static struct ucafs_mod ucafs_m_device;
struct ucafs_mod * dev = &ucafs_m_device;

static atomic_t ucafs_m_available = ATOMIC_INIT(1);

/* major & minor numbers for our modules */
static int uc_mod_major = 0;
static int uc_mod_minor = 0;

static int uc_mod_devno = 0;

static int ucafs_module_is_mounted = 0;

mid_t msg_counter = 0;

static int
ucafs_m_open(struct inode * inode, struct file * fp)
{
    if (!atomic_dec_and_test(&ucafs_m_available)) {
        atomic_inc(&ucafs_m_available);
        return -EBUSY;
    }

    dev->daemon = current;
    fp->private_data = dev;

    return 0;
}

static int
ucafs_m_release(struct inode * inode, struct file * fp)
{
    mutex_lock_interruptible(&dev->mut);
    dev->daemon = NULL;
    atomic_inc(&ucafs_m_available);
    mutex_unlock(&dev->mut);

    return 0;
}

#if 0
static int
ucafs_m_poll(struct file * fp, poll_table * wait)
{
    size_t mask = 0;

    mutex_lock(&dev->mut);
    poll_wait(fp, &dev->kq, wait);
    if (dev->avail_read) {
        mask |= POLLIN | POLLRDNORM;
    }

    if (dev->avail_write) {
        mask |= POLLIN | POLLRDNORM;
    }
    mutex_unlock(&dev->mut);

    return mask;
}
#endif

static ssize_t
ucafs_m_read(struct file * fp, char __user * buf, size_t count, loff_t * f_pos)
{
    size_t len;

    if (mutex_lock_interruptible(&dev->mut)) {
        return -ERESTARTSYS;
    }

    while (dev->avail_read == 0) {
        mutex_unlock(&dev->mut);
        // the current process waits
        if (wait_event_interruptible(dev->rq, dev->avail_read > 0)) {
            return -ERESTARTSYS;
        }

        // we are woken up, reaquire the lock
        if (mutex_lock_interruptible(&dev->mut)) {
            return -ERESTARTSYS;
        }
    }

    /* we can ship data to userspace */
    len = dev->msg_len;
    count = min(count, dev->avail_read);

    if (copy_to_user(buf, dev->outb + (len - dev->avail_read), count)) {
        mutex_unlock(&dev->mut);
        return -EFAULT;
    }

    printk(KERN_INFO "wrote %zu bytes\n", count);

    /* update the pointer */
    dev->avail_read -= count;
    // TODO zero the buffer

    mutex_unlock(&dev->mut);
    return count;
}

static ssize_t
ucafs_m_write(struct file * fp,
              const char __user * buf,
              size_t count,
              loff_t * f_pos)
{
    if (mutex_lock_interruptible(&dev->mut)) {
        return -ERESTARTSYS;
    }

    while (dev->avail_write == 0) {
        mutex_unlock(&dev->mut);

        if (wait_event_interruptible(dev->wq, dev->avail_write > 0)) {
            return -ERESTARTSYS;
        }

        if (mutex_lock_interruptible(&dev->mut)) {
            return -ERESTARTSYS;
        }
    }

    /* copy the message in full */
    if (copy_from_user(dev->inb, buf, count)) {
        mutex_unlock(&dev->mut);
        return -EFAULT;
    }

    printk(KERN_INFO "read %zu bytes\n", count);

    dev->avail_write -= count;

    mutex_unlock(&dev->mut);
    wake_up_interruptible(&dev->kq);
    return count;
}

typedef struct {
    XDR xdrs;
    char data[0];
} xdr_data_t;

int
ucafs_mod_send(uc_msg_type_t type, XDR * xdrs, XDR ** pp_rsp, int * p_err)
{
    mid_t id;
    xdr_data_t * rsp_data;
    ucrpc_msg_t * msg;
    int err = -1, msg_len = (xdrs->x_private - xdrs->x_base);
    *p_err = -1;

    if (UCAFS_IS_OFFLINE) {
        mutex_unlock(&dev->mut);
        return -1;
    }

    RX_AFS_GUNLOCK();

    /* send the message */
    msg = (ucrpc_msg_t *)dev->outb;
    msg->type = type;
    msg->msg_id = id = ucrpc__genid();
    msg->len = msg_len;
    dev->avail_read += MSG_SIZE(msg);
    dev->msg_len = MSG_SIZE(msg);

    while (1) {
        DEFINE_WAIT(wait);
        if (UCAFS_IS_OFFLINE) {
            printk(KERN_ERR "process is offline :(");
            goto out;
        }

        mutex_unlock(&dev->mut);
        wake_up_interruptible(&dev->rq);

        /* sleep the kernel thread */
        prepare_to_wait(&dev->kq, &wait, TASK_INTERRUPTIBLE);

        /* the buffer is "empty", nothing to read */
        if (dev->avail_write == dev->buffersize) {
            schedule();
        }

        finish_wait(&dev->kq, &wait);

        /* now read the buffer */
        if (mutex_lock_interruptible(&dev->mut)) {
            printk(KERN_ERR "mutex_lock_interruptible failed\n");
            goto out;
        }

        msg = (ucrpc_msg_t *)dev->inb;
        if (msg->ack_id == id) {
            dev->avail_write += MSG_SIZE(msg);

            /* allocate the response data */
            rsp_data = kmalloc(sizeof(xdr_data_t) + msg->len + 1, GFP_KERNEL);
            if (rsp_data == NULL) {
                *p_err = msg->status;
                *pp_rsp = NULL;
                ERROR("allocation error\n");
                goto out;
            }

            /* instantiate everything */
            memcpy(rsp_data->data, &msg->payload, msg->len);
            xdrmem_create(&rsp_data->xdrs, rsp_data->data, msg->len, XDR_DECODE);

            *p_err = msg->status;
            *pp_rsp = &rsp_data->xdrs;

            err = 0;
            break;
        }
    }

out:
    RX_AFS_GLOCK();
    mutex_unlock(&dev->mut);
    return err;
}

static int
ucafs_p_show(struct seq_file * sf, void * v)
{
    if (dev->daemon == NULL) {
        seq_printf(sf, "daemon pid: %d\n", (int)dev->daemon->pid);
    } else {
        seq_printf(sf, "daemon offline :(\n");
    }

    seq_printf(sf, "avail_read=%zu, avail_write=%zu\n", dev->avail_read,
               dev->avail_write);

    return 0;
}

static int
ucafs_p_open(struct inode * inode, struct file * file)
{
    return single_open(file, ucafs_p_show, NULL);
}

const struct file_operations ucafs_mod_fops = {.owner = THIS_MODULE,
                                               .open = ucafs_m_open,
                                               .release = ucafs_m_release,
                                               .write = ucafs_m_write,
                                               .read = ucafs_m_read};

const struct file_operations ucafs_proc_fops = {.open = ucafs_p_open,
                                                .read = seq_read,
                                                .llseek = seq_lseek,
                                                .release = single_release};

int
ucafs_mod_init(void)
{
    int ret;

    if (ucafs_module_is_mounted) {
        printk(KERN_NOTICE "ucafs_mod is already mounted\n");
        return 0;
    }

    ret = alloc_chrdev_region(&uc_mod_devno, 0, 1, "ucafs_mod");
    if (ret) {
        printk(KERN_NOTICE "register_chrdev_region failed %d\n", ret);
        return ret;
    }

    /* lets now initialize the data structures */
    memset(dev, 0, sizeof(struct ucafs_mod));
    init_waitqueue_head(&dev->kq);
    init_waitqueue_head(&dev->rq);
    init_waitqueue_head(&dev->wq);
    mutex_init(&dev->mut);

    if ((dev->buffer = UCMOD_BUFFER_ALLOC()) == NULL) {
        printk(KERN_ERR "buffer allocation failed\n");
        return -1;
    }

    dev->buffersize = UCMOD_BUFFER_SIZE >> 1;
    dev->end = dev->buffer + UCMOD_BUFFER_SIZE;
    dev->outb = dev->buffer;
    dev->inb = dev->buffer + dev->buffersize;
    dev->avail_read = 0;
    dev->avail_write = dev->buffersize;

    /* lets setup the character device */
    uc_mod_major = MAJOR(uc_mod_devno);
    uc_mod_minor = MINOR(uc_mod_devno);
    cdev_init(&dev->cdev, &ucafs_mod_fops);
    dev->cdev.owner = THIS_MODULE;
    if ((ret = cdev_add(&dev->cdev, uc_mod_devno, 1))) {
        printk(KERN_ERR "adding %d cdev failed: %d\n", uc_mod_devno, ret);
        return ret;
    }

    ucafs_module_is_mounted = 1;
    printk(KERN_INFO "ucafs_mod: mounted major=%d, minor=%d :)\n", uc_mod_major,
           uc_mod_minor);

    proc_create_data("ucafs_mod", 0, NULL, &ucafs_proc_fops, NULL);

    return 0;
}

int
ucafs_mod_exit(void)
{
    // TODO just free the buffers
    return 0;
}
