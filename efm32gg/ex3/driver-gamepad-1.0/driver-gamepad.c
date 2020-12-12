#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/ioport.h>

#include <asm/io.h>
#include <asm/uaccess.h>
#include <asm/signal.h>
#include <asm/siginfo.h>

#include "efm32gg.h"

static int __init gamepad_init(void);
static void __exit gamepad_cleanup(void);
static irqreturn_t interrupt_handler(int irq, void *dev_id);
static int gamepad_fasync(int fd, struct file *file, int mode);
static int gamepad_open(struct inode *inode, struct file *file);
static int gamepad_release(struct inode *inode, struct file *file);
static ssize_t gamepad_read(struct file *file, char __user *buffer,
    size_t size, loff_t *offset);
static ssize_t gamepad_write(struct file *file, const char __user *buffer,
    size_t size, loff_t *offset);
void destroy_device(void);

#define DRIVER_NAME "gamepad"

struct cdev device;
static dev_t device_number;
struct class *device_class;

struct fasync_struct *async_queue;

static struct file_operations driver_fops = {
    .owner   = THIS_MODULE,
    .read    = gamepad_read,
    .write   = gamepad_write,
    .open    = gamepad_open,
    .release = gamepad_release,
    .fasync  = gamepad_fasync
};

module_init(gamepad_init);
module_exit(gamepad_cleanup);

MODULE_DESCRIPTION("EFM32GG gamepad driver");
MODULE_LICENSE("GPL");

static int __init gamepad_init(void)
{
    struct resource *mem_region = NULL;
    int status = 0;

    printk(KERN_INFO "Loading gamepad driver...\n");

    /* Allocate device number */
    status = alloc_chrdev_region(&device_number, 0, 1, DRIVER_NAME);
    if (status < 0)
    {
        printk(KERN_ALERT "Failed to allocate device number\n");
        return 1;
    }

    /* Setup device */
    cdev_init(&device, &driver_fops);
    device.owner = THIS_MODULE;
    cdev_add(&device, device_number, 1);

    /* Setup device class */
    device_class = class_create(THIS_MODULE, DRIVER_NAME);
    device_create(device_class, NULL, device_number, NULL, DRIVER_NAME);

    /* Request memory region */
    mem_region = request_mem_region(GPIO_PC_DIN, 1, DRIVER_NAME);
    if (mem_region == NULL)
    {
        printk(KERN_ALERT "Failed to allocate memory region\n");
        destroy_device();
        return 1;
    }

    /* Configure gamepad */
    iowrite32(0x33333333, GPIO_PC_MODEL);
    iowrite32(0xff, GPIO_PC_DOUT);

    /* Configure interrupt generation from gamepad */
    iowrite32(0xff, GPIO_IEN);
    iowrite32(0x22222222, GPIO_EXTIPSELL);
    iowrite32(0xff, GPIO_EXTIFALL);
    iowrite32(0xff, GPIO_IEN);
    iowrite32(0xff, GPIO_IFC);

    /* Configure interrupt handling for GPIO_ODD and GPIO_EVEN */
    request_irq(17, (irq_handler_t)interrupt_handler, 0, DRIVER_NAME,
        &device);
    request_irq(18, (irq_handler_t)interrupt_handler, 0, DRIVER_NAME,
        &device);

    printk(KERN_INFO "Gamepad driver loaded!\n");

    return 0;
}

static void __exit gamepad_cleanup(void)
{
    printk(KERN_INFO "Unloading gamepad driver...\n");

    iowrite32(0x0000, GPIO_IEN);
    free_irq(17, &device);
    free_irq(18, &device);

    release_mem_region(GPIO_PC_DIN, 1);

    destroy_device();

    printk(KERN_INFO "Gamepad driver unloaded!\n");
}

/* Dispatches SIGIO to list of recipients */
static irqreturn_t interrupt_handler(int irq_no, void *dev_id)
{
    iowrite32(0xff, GPIO_IFC);

    if (async_queue)
        kill_fasync(&async_queue, SIGIO, POLL_IN);

    return IRQ_HANDLED;
}

/* Copies the first 'size' bytes from GPIO_PC_DIN to 'buffer' */
static ssize_t gamepad_read(struct file *file, char __user *buffer,
    size_t size, loff_t *offset)
{
    int8_t data = ioread32(GPIO_PC_DIN);
    copy_to_user(buffer, &data, size);
    return 0;
}

/* Registers the calling process to list of SIGIO recipients */
static int gamepad_fasync(int fd, struct file *file, int mode)
{
    return fasync_helper(fd, file, mode, &async_queue);
}

static ssize_t gamepad_write(struct file *file, const char __user *buffer,
    size_t size, loff_t *offset)
{
    return 0;
}

static int gamepad_open(struct inode *inode, struct file *file)
{
    return 0;
}

static int gamepad_release(struct inode *inode, struct file *file)
{
    return 0;
}

void destroy_device(void)
{
    device_destroy(device_class, device_number);
    class_destroy(device_class);
    cdev_del(&device);
    unregister_chrdev_region(device_number, 1);
}
