#include <linux/module.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
//#include <linux/flex_array.h>

#define DEVICE_NAME "var4"
#define ARR_MIN_SIZE 128

static dev_t first;
static struct cdev c_dev;
static struct class *cl;

static char *dev_read_buffer;
//DEFINE_FLEX_ARRAY(spaces_stat, sizeof(size_t), ARR_MIN_SIZE);

static size_t *spaces_stat;
static int position;
static int capacity;
static struct proc_dir_entry *stat_file;

static int my_dev_open(struct inode *i, struct file *f)
{
        pr_info("Driver: open()\n");
        return 0;
}

static int my_dev_close(struct inode *i, struct file *f)
{
        pr_info("Driver: close()\n");
        return 0;
}

static ssize_t my_dev_read(struct file *f, char __user *buf, size_t len, loff_t *off)
{
        int i;

        pr_info("Driver: read()\n");
        for (i=0; i<position; i++) {
                pr_info("%ld\n", spaces_stat[i]);
        }

        return 0;
}

static ssize_t my_dev_write(struct file *f, const char __user *buf,  size_t len, loff_t *off)
{
        int i;
        size_t *spaces_stat_new;
        size_t count_spaces;

        pr_info("Driver: write()\n");
        count_spaces = 0;

        dev_read_buffer = kmalloc(sizeof(char)*len, GFP_KERNEL);
        if (!dev_read_buffer)
                return -ENOMEM;

        if (copy_from_user(dev_read_buffer, buf, len))
                return -EFAULT;

        for (i=0; i<len; ++i) {
                if (dev_read_buffer[i] == ' ')
                        count_spaces++;
        }
        kfree(dev_read_buffer);

        //flex_array_put(spaces_stat, position, count_spaces, GFP_KERNEL);
        if (position >= capacity) {
                capacity *= 2;
                spaces_stat_new = kmalloc(sizeof(size_t)*capacity, GFP_KERNEL);
                if (!spaces_stat_new) {
                        pr_alert("Failed to reinitialize statistics array\n");
                        return len;
                }
                memmove(spaces_stat_new, spaces_stat, (capacity/2)*sizeof(size_t));
                kfree(spaces_stat);
                spaces_stat = spaces_stat_new;
        }
        spaces_stat[position] = count_spaces;
        position++;

        return len;
}

static struct file_operations mychdev_fops = {
        .owner = THIS_MODULE,
        .open = my_dev_open,
        .release = my_dev_close,
        .read = my_dev_read,
        .write = my_dev_write
};

static ssize_t my_proc_write(struct file *file_ptr, const char __user *ubuffer, size_t buf_length, loff_t *offset)
{
        pr_info("Proc: write() - nothing required to do here");
        return buf_length;
}

static ssize_t my_proc_read(struct file *file_ptr, char __user *ubuffer, size_t buf_length, loff_t *offset)
{
        int i;
        char *out_buf;
        char *out_p;

        pr_info("Proc: read()");

        if (*offset > 0)
                return 0;

        out_buf = kmalloc(4*position*sizeof(char), GFP_KERNEL);
        if (!out_buf) {
                pr_alert("Failed to allocate buffer message\n");
                kfree(out_buf);
                return -ENOMEM;
        }

        out_p = out_buf;
        for (i=0; i<position; i++) {
                size_t next = spaces_stat[i];
                out_p += sprintf(out_p, "%ld\n", next);
        }

        if (copy_to_user(ubuffer, out_buf, out_p - out_buf)) {
                pr_alert("Failed to copy buffer message\n");
                kfree(out_buf);
                return -EFAULT;
        }
        *offset += out_p - out_buf;
        kfree(out_buf);
        return *offset;
}

static const struct file_operations proc_file_ops = {
        .owner = THIS_MODULE,
        .read = my_proc_read,
        .write = my_proc_write
};

static int __init my_init(void)
{
        if (alloc_chrdev_region(&first, 0, 1, DEVICE_NAME) < 0)
                return -1;

        if (!(cl = class_create(THIS_MODULE, "chardev"))) {
                unregister_chrdev_region(first, 1);
                return -1;
        }
        if (!(device_create(cl, NULL, first, NULL, DEVICE_NAME))) {
                class_destroy(cl);
                unregister_chrdev_region(first, 1);
                return -1;
        }
        cdev_init(&c_dev, &mychdev_fops);
        if (cdev_add(&c_dev, first, 1) == -1) {
                device_destroy(cl, first);
                class_destroy(cl);
                unregister_chrdev_region(first, 1);
                return -1;
        }

        stat_file = proc_create(DEVICE_NAME, 0666, NULL, &proc_file_ops);
        if (!stat_file) {
                pr_alert("Failed to create proc file\n");
        }

        position = 0;
        capacity = ARR_MIN_SIZE;
        spaces_stat = kmalloc(sizeof(size_t)*ARR_MIN_SIZE, GFP_KERNEL);
        if (!spaces_stat)
                pr_alert("Failed to initialize statistics array\n");

        pr_info("Module initialized\n");
        return 0;
}

static void __exit my_exit(void)
{
        if (spaces_stat)
                kfree(spaces_stat);
        //flex_array_free(spaces_stat);
        if (stat_file)
                proc_remove(stat_file);
        cdev_del(&c_dev);
        device_destroy(cl, first);
        class_destroy(cl);
        unregister_chrdev_region(first, 1);
        pr_info("Module released\n");
}

module_init(my_init);
module_exit(my_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Margarita Blinova & Leonid Geller");
MODULE_DESCRIPTION("A kernel module for IO systems lab 1");
