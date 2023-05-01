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
#include "aesd_ioctl.h"
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

/*
f_pos will always start at 0 when using `echo "blah" > /dev/aesdchar`
    f_pos is also included in file *filp
    why
        passed *f_pos is modified by read/write implemetation
        use the *filp f_pos if you don't intend to manipulate and see the value
        that was passed in to the function, i.e. starting position of the file
        pointer
        if you do want to modify then use the one that was passed in *f_pos
        so, filp->f_pos is modififed by underlying shared drtivers bases on read
        /write OR update it via llseek function

implement the llseek function
    this will modify the *filp f_pos
    and also modify the filp f_pos in an ioctl (wouldn't typically be done this
    way) - this is to easier test the ioctl implementation

    How to interact in user space
        off_t lseek(int fd, off_t offset, int whence)
        SEEK_SET
            use the specified offset as the file position
        SEEK_CUR
            increment or decrement file position
        SEEK_END
           use EOF as file postion

    llseek implementation - implements the lseek and llseek system calls
    why two different system calls, lseek and _llseek?
        llseek is guranteed to support long long offset sizes

    use the llseek pointer

    if the llseek method is missing from the device's operations, the default
    implementation in the kernel performs seek by modifying filp->fpos
    for the lseek system call to work correctly the read and write methods must
    cooperate by using and updating the offset item they receive as an argument

    llseek driver implementation
        if the llseek method is missing from the device's operations, the
        default implemetation is the kernel performs seek by modifying filp->fpos

        - see screenshot of fixed_size_llseek() implementation

        several wrappers around generic llseek which seek for you which you can
        call from your llseek method
        this one uses a size you provide, You can just pass all these seek
        arguments directly into the kernel and it will update the file position
        for you as long as you just provide the size.
        What should we use for size?
            the total size of all content of the circular buffer

llseek assignment 9 options
    leave llseek null and use the default llseek
        would require supporting seek in write() (not assuming every write
        appends) which isn't an assignment requirement - i have not experimented
        this myself - more challenging

    add your own llseek function, with locking and LOGGING, but use
    fixed_size_llseek for logic. (suggested approach)

    implement your own llseek function seperate from fixed_size_llseek handling
    each of the "whence" cases
*/
loff_t llseek(struct file *filp, loff_t offset, int whence)
{
    struct aesd_dev *dev = filp->private_data;
    loff_t file_pos;

    if (mutex_lock_interruptible(&dev->lock)){
        return -ERESTARTSYS;
    }

    file_pos = fixed_size_llseek(filp, offset, whence, dev->buffer.total_size);
    filp->f_pos = file_pos;

    mutex_unlock(&dev->lock);

    return file_pos;
}

/*
    adjusting the file offset from ioctl
        what this command could look like
        static long aesd_adjust_file_offset(
            struct file *filp,
            unsighned int write_cmd,
            unsigned int write_cmd_offset);

        check for valid write_cmd and write_cmd_offset values
            When would values be invalid?
                haven't written this command yet
                out of range cmd (11)
                write_cmd_offset is >= size of command
        calculate the start offset to write_cmd
            add length of each write between the output pointer and write_cmd
        add write_cmd_offset
        save as filp->f_pos
*/
static long aesd_adjust_file_offset(
    struct file *filp,
    unsigned int write_cmd,
    unsigned int write_cmd_offset)
{
    long retval = 0;
    struct aesd_dev *dev = filp->private_data;
    long total_len;
    int i;

    if (mutex_lock_interruptible(&dev->lock)){
        return -ERESTARTSYS;
    }

    if(write_cmd >= AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED || write_cmd < 0){
        retval = -EINVAL;
        goto out;
    }
    if(dev->buffer.entry[write_cmd].buffptr == NULL ||
       dev->buffer.entry[write_cmd].size <= write_cmd_offset){
        retval = -EINVAL;
        goto out;
    }

    for(i = 0; i<AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED; i++){
        total_len += dev->buffer.entry[i].size;
        if (i == write_cmd)
        {
            break;
        }

    }
    filp->f_pos = total_len;

    out:
        mutex_unlock(&dev->lock);

    return retval;
}

/*
read/write method and f_pos
    for the lseek system call to work correctly the read and write methods must
    cooperate by using and updating the offset item they receive as an argument
        read function:
            must set *f_pos to *f_pos + retcount where retcount is the number of
            bytes read
        write function:
            must set *f_pos to *f_pos + retcount where retcount is the number of
            bytes written
*/


/*
ioctl implementation
    add and aesd_ioctl.h file you can share with your aesdsocket implementation
    see provided aesd_ioctl.h file at:
        github/aesd-assignments/blob/assignment9/aesd-char-driver/aesd_ioctl.h
        magic number defined
        seekto command defined for ioctl
        seekto struct part of the ioctl - tells what command we want to seek into
            and the byte offset inside that command we want to seek into

    ioctl user space implementation - aesdsocket
        see screenshot for code
        #include the aesd_ioctl.h file
        use the ioctl command with fd representing the driver
        pass the filled in structure to the driver via ioctl

        struct aesd_seekto seekto;
        seekto.write_cmd = write_cmd;
        seekto.write_cmd_offset = offset;
        int resutl_ret = ioctl(fd, AESDCHAR_IOCSEEKTO, &seekto);

        My aesdsocket uses a FILE* to access my driver, How do I get the fd used
        for ioctl?
            Use fileno()
                int fileno(FILE *stream);

    ioctl implementation - driver
        see screenshot for code
        #include the aesd_ioctl.h file
        use copy_from_user to obtain the value from userspace

        the ioctl handler in this driver function will look something like this
        case AESDCHAR_IOCSEEKTO:
            struct aesd_seekto seekto;
            if(copy_from_user(&seekto, (const void __user *)arg, sizeof(seekto)) != 0){
                retval = EFAULT;
            }
            else{
                retval = aesd_adjust_file_offset(
                    filp,
                    seekto.write_cmd,
                    seekto.write_cmd_offset
                );
            }
            break;
*/
loff_t ioctl(struct file *filp, unsigned long cmd, unsigned long arg)
{
    struct aesd_seekto seekto;
    struct aesd_dev *dev = filp->private_data;
    long retval;

    if (mutex_lock_interruptible(&dev->lock)){
        return -ERESTARTSYS;
    }

    switch(cmd) {
        case AESDCHAR_IOCSEEKTO:
            if(copy_from_user(&seekto, (const void __user *)arg, sizeof(seekto)) != 0){
                retval = EFAULT;
                goto out;
            }
            else{
                retval = aesd_adjust_file_offset(
                    filp,
                    seekto.write_cmd,
                    seekto.write_cmd_offset
                );
            }
            break;
        default:
            retval = -ENOTTY;
            goto out;
    }

    out:
        mutex_unlock(&dev->lock);

    return retval;
}

/*
New Test scripts
    drivertest.sh and sockettest.sh scripts hsould continue to pass with the
    changes in this assignment.

    new scripts assignment9/drivertest.sh and assignment9/sockettest.sh should
    also succeed
        you will need to stop.restart your driver when running the sockettest
        scripts (since we aren't supporting command delete).
*/
/*
seeking on the command line
    use the dd utility to perform seeking on the command line
        see assignment9 test scripts for an example of this
*/

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
