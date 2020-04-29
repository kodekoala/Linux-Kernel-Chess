#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
//#include <linux/device.h>
#include <linux/cdev.h>
#include "chess.h"

#define DRIVER_AUTHOR "Yousuf"
#define DRIVER_DESC "Chess game driver"

MODULE_LICENSE("GPL");
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_SUPPORTED_DEVICE("Chess")

static dev_t device_number;
static struct cdev the_cdev;
static struct class* the_class = NULL;

static struct file_operations fops = {
    .owner = THIS_MODULE,
    .read = chess_read,
    .write = chess_write,
    .open = chess_open,
    .release = chess_release
};

static char *chess_proc_devnode(struct device *dev, umode_t *mode){
    if (mode != NULL){
        *mode = 0666; 
    }
    //return MODULE_NAME"_dev";
    return kasprintf(GFP_KERNEL, MODULE_NAME);
    //return NULL;
}

static int __init chess_init(void)
{
    LOG_INFO("Module loaded\n");
    //This will show under /proc/devices
    if (alloc_chrdev_region(&device_number, 0, 1, MODULE_NAME"_proc") < 0){
        goto r_return;
    }

    // This will show up under ls /sys/class
    the_class = class_create(THIS_MODULE, MODULE_NAME"_sys");
    if (the_class == NULL){
        goto r_class;
    }

    the_class->devnode = chess_proc_devnode;

    // This will show up under /dev
    if (device_create(the_class, NULL, device_number, NULL, MODULE_NAME"_dev") == NULL)
    {
        goto r_device;
    }

    cdev_init(&the_cdev, &fops);

    if (cdev_add(&the_cdev, device_number, 1) < 0){
        goto r_cdev;
    }

    return 0;

r_cdev:
    device_destroy(the_class, device_number);
    cdev_del(&the_cdev);

r_device:
    class_destroy(the_class);

r_class:
    unregister_chrdev_region(device_number, 1);

r_return:
    return -1;
}

static void __exit chess_exit(void)
{
    LOG_INFO("Module unloaded\n");
    device_destroy(the_class, device_number);
    cdev_del(&the_cdev);
    class_destroy(the_class);
    unregister_chrdev_region(device_number, 1);
}

module_init(chess_init);
module_exit(chess_exit);
