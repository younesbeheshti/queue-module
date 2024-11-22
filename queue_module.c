#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>
#include <linux/wait.h>
#include <linux/slab.h>
#include <linux/sched.h>

#define QUEUE_CAPACITY 10
#define DRIVER_NAME "charQueue"
#define DRIVER_CLASS "charQueueClass"

MODULE_LICENSE("GPL");
MODULE_VERSION("1.0");

static int driverMajor;
static char messageQueue[QUEUE_CAPACITY];
static int headIndex = 0;
static int tailIndex = 0;
static int queueSize = 0;
static int enableBlocking = 1;

static DECLARE_WAIT_QUEUE_HEAD(readWaitQueue);
static DECLARE_WAIT_QUEUE_HEAD(writeWaitQueue);

static struct class *queueClass = NULL;
static struct device *queueDevice = NULL;

// Function prototypes
static int queue_open(struct inode *, struct file *);
static int queue_release(struct inode *, struct file *);
static ssize_t queue_read(struct file *, char *, size_t, loff_t *);
static ssize_t queue_write(struct file *, const char *, size_t, loff_t *);

static struct file_operations fops = {
    .open = queue_open,
    .read = queue_read,
    .write = queue_write,
    .release = queue_release,
};

// Initialize the module
static int __init queue_init(void) {
    printk(KERN_INFO "charQueue: Initializing the driver\n");

    // Register the character device
    driverMajor = register_chrdev(0, DRIVER_NAME, &fops);
    if (driverMajor < 0) {
        printk(KERN_ERR "charQueue: Failed to register a major number\n");
        return driverMajor;
    }
    printk(KERN_INFO "charQueue: Registered with major number %d\n", driverMajor);

    // Create device class
    queueClass = class_create(THIS_MODULE, DRIVER_CLASS);
    if (IS_ERR(queueClass)) {
        unregister_chrdev(driverMajor, DRIVER_NAME);
        printk(KERN_ERR "charQueue: Failed to register device class\n");
        return PTR_ERR(queueClass);
    }
    printk(KERN_INFO "charQueue: Device class registered successfully\n");

    // Create the device
    queueDevice = device_create(queueClass, NULL, MKDEV(driverMajor, 0), NULL, DRIVER_NAME);
    if (IS_ERR(queueDevice)) {
        class_destroy(queueClass);
        unregister_chrdev(driverMajor, DRIVER_NAME);
        printk(KERN_ERR "charQueue: Failed to create device\n");
        return PTR_ERR(queueDevice);
    }
    printk(KERN_INFO "charQueue: Device created successfully\n");

    return 0;
}

// Cleanup the module
static void __exit queue_exit(void) {
    device_destroy(queueClass, MKDEV(driverMajor, 0));
    class_destroy(queueClass);
    unregister_chrdev(driverMajor, DRIVER_NAME);
    printk(KERN_INFO "charQueue: Exiting the module\n");
}

// Open device file
static int queue_open(struct inode *inodep, struct file *filep) {
    printk(KERN_INFO "charQueue: Device opened\n");
    return 0;
}

// Release device file
static int queue_release(struct inode *inodep, struct file *filep) {
    printk(KERN_INFO "charQueue: Device closed\n");
    return 0;
}

// Read from the queue
static ssize_t queue_read(struct file *filep, char *buffer, size_t len, loff_t *offset) {
    int err, bytesToRead = 1;

    // Wait if the queue is empty and blocking is enabled
    if (queueSize == 0) {
        if (enableBlocking) {
            wait_event_interruptible(readWaitQueue, queueSize > 0);
        } else {
            return 0; // Non-blocking mode
        }
    }

    // Read one character
    char data = messageQueue[headIndex];
    headIndex = (headIndex + 1) % QUEUE_CAPACITY;
    queueSize--;

    // Copy data to user space
    err = copy_to_user(buffer, &data, bytesToRead);
    if (err == 0) {
        printk(KERN_INFO "charQueue: Read %d byte(s)\n", bytesToRead);
        wake_up_interruptible(&writeWaitQueue);
        return bytesToRead;
    } else {
        printk(KERN_ERR "charQueue: Failed to send data to user space\n");
        return -EFAULT;
    }
}

// Write to the queue
static ssize_t queue_write(struct file *filep, const char *buffer, size_t len, loff_t *offset) {
    int err, bytesToWrite = 1;

    // Wait if the queue is full and blocking is enabled
    if (queueSize == QUEUE_CAPACITY) {
        if (enableBlocking) {
            wait_event_interruptible(writeWaitQueue, queueSize < QUEUE_CAPACITY);
        } else {
            return 0; // Non-blocking mode
        }
    }

    // Write one character
    char data;
    err = copy_from_user(&data, buffer, bytesToWrite);
    if (err == 0) {
        messageQueue[tailIndex] = data;
        tailIndex = (tailIndex + 1) % QUEUE_CAPACITY;
        queueSize++;

        printk(KERN_INFO "charQueue: Written %d byte(s)\n", bytesToWrite);
        wake_up_interruptible(&readWaitQueue);
        return bytesToWrite;
    } else {
        printk(KERN_ERR "charQueue: Failed to receive data from user space\n");
        return -EFAULT;
    }
}

module_param(enableBlocking, int, 0644);
MODULE_PARM_DESC(enableBlocking, "Enable blocking (1) or non-blocking mode (0)");

module_init(queue_init);
module_exit(queue_exit);
