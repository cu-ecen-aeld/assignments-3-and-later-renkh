/**
 * @file aesdchar.c
 * @brief Functions and data related to the AESD char driver implementation
 *
 * Based on the implementation of the "scull" device driver, found in
 * Linux Device Drivers example code.
 *
 * @author Dan Walkes
 * @date 2019-10-22
 * @copyright Copyright (c) 2019
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/fs.h> // file_operations
#include <linux/slab.h> // kmalloc
#include <linux/uaccess.h> // copy_to_user
#include "aesdchar.h"
int aesd_major =   0; // use dynamic major
int aesd_minor =   0;

MODULE_AUTHOR("Renat Khalikov");
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;

int aesd_open(struct inode *inode, struct file *filp)
{
    struct aesd_dev *dev;
    PDEBUG("open");
    dev = container_of(inode->i_cdev, struct aesd_dev, cdev);
    filp->private_data = dev;
    return 0;
}

int aesd_release(struct inode *inode, struct file *filp)
{
    PDEBUG("release");
    /**
     * TODO: handle release
     */
    // this will tie in with cleanup_module
    return 0;
}

ssize_t aesd_read(struct file *filp, char __user *buf, size_t count,
                loff_t *f_pos)
{
    ssize_t retval = 0;
    // the filp file pointer will have a private_data member that can be used
    // to get a pointer to aesd_dev struct
    struct aesd_dev *dev = filp->private_data;

    // the buf buffer will be used to fill read from userspace, can't access the
    // buffer directly, instead use copy_to_user to copy from kernel space to
    // user space
    struct aesd_buffer_entry *buffer_entry;
    size_t entry_offset_byte_rtn;
    size_t num_of_writes;
    int not_copied;

    PDEBUG("read %zu bytes with offset %lld",count,*f_pos);

    if (mutex_lock_interruptible(&dev->lock)){
        return -ERESTARTSYS;
    }

    buffer_entry = aesd_circular_buffer_find_entry_offset_for_fpos(
        &dev->buffer,
        *f_pos,
        &entry_offset_byte_rtn
    );
    if (buffer_entry == NULL){
        *f_pos = 0;
        goto out;
    }

    // count - max number of writes to the buffer, may want to write less for this
    num_of_writes = buffer_entry->size - entry_offset_byte_rtn;
    if(count < num_of_writes){
        num_of_writes = count;
    }

    not_copied = copy_to_user(
        buf,
        &buffer_entry->buffptr[entry_offset_byte_rtn],
        num_of_writes
    );

    // partial read rule
    retval = num_of_writes - not_copied;
    *f_pos += retval;

    out:
        mutex_unlock(&dev->lock);

    return retval;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos)
{
    ssize_t retval = -ENOMEM;
    struct aesd_dev *dev = filp->private_data;
    ssize_t num_not_copied;
    ssize_t i;

    PDEBUG("write %zu bytes with offset %lld",count,*f_pos);

    if (mutex_lock_interruptible(&dev->lock)){
        return -ERESTARTSYS;
    }

    dev->entry.buffptr = krealloc(
        dev->entry.buffptr,
        dev->entry.size + count,
        GFP_KERNEL
    );

    // append to the command being written when there's no newline received
    num_not_copied = copy_from_user(
        (void *)&dev->entry.buffptr[dev->entry.size],
        buf,
        count
    );

    retval = count - num_not_copied;
    dev->entry.size += retval;

    for (i = 0; i < dev->entry.size; i++){
        if(dev->entry.buffptr[i] == '\n'){
            // write to the command buffer when a newline is received
            aesd_circular_buffer_add_entry(&dev->buffer, &dev->entry);
            dev->entry.buffptr = NULL;
            dev->entry.size = 0;
        }
    }

    mutex_unlock(&dev->lock);
    return retval;
}
struct file_operations aesd_fops = {
    .owner =    THIS_MODULE,
    .read =     aesd_read,
    .write =    aesd_write,
    .open =     aesd_open,
    .release =  aesd_release,
};

static int aesd_setup_cdev(struct aesd_dev *dev)
{
    int err, devno = MKDEV(aesd_major, aesd_minor);

    cdev_init(&dev->cdev, &aesd_fops);
    dev->cdev.owner = THIS_MODULE;
    dev->cdev.ops = &aesd_fops;
    err = cdev_add (&dev->cdev, devno, 1);
    if (err) {
        printk(KERN_ERR "Error %d adding aesd cdev", err);
    }
    return err;
}



int aesd_init_module(void)
{
    dev_t dev = 0;
    int result;
    result = alloc_chrdev_region(&dev, aesd_minor, 1,
            "aesdchar");
    aesd_major = MAJOR(dev);
    if (result < 0) {
        printk(KERN_WARNING "Can't get major %d\n", aesd_major);
        return result;
    }
    memset(&aesd_device,0,sizeof(struct aesd_dev));

    /**
     * TODO: initialize the AESD specific portion of the device
     */
    // initalize all the things added to struct aesd_dev
    // the locking primative
    //
    aesd_circular_buffer_init(&aesd_device.buffer);
    mutex_init(&aesd_device.lock);

    result = aesd_setup_cdev(&aesd_device);

    if( result ) {
        unregister_chrdev_region(dev, 1);
    }
    return result;

}

void aesd_cleanup_module(void)
{
    uint8_t index;
    struct aesd_buffer_entry *entry;
    dev_t devno = MKDEV(aesd_major, aesd_minor);

    cdev_del(&aesd_device.cdev);

    /**
     * TODO: cleanup AESD specific poritions here as necessary
     */
    // balance whatever youre doing in the init_module with what youre doing in
    // here, aka allocate memory == free memory
    // initialize primitives == free locking primitives
    AESD_CIRCULAR_BUFFER_FOREACH(entry, &aesd_device.buffer, index) {
        kfree(entry->buffptr);
    }
    mutex_destroy(&aesd_device.lock);

    unregister_chrdev_region(devno, 1);
}



module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
