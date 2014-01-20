/*
 * my_device_0,/dev/prod, it takes data and stores
 * in a linked list
 * my_device_1 /dev/cons, it takes data from linked
 * list and returns to user
 */

#include "simple.h"

wait_queue_head_t prod_que, cons_que;
struct semaphore prod_sem;
struct semaphore cons_sem;

static struct class *cl;
static int simple_major;
static int simple_minor;
static struct cdev simple_cdev[MAX_SIMPLE_DEV];

DECLARE_KFIFO(fifo, char, FIFO_SIZE);/*Circular buffer using kfifo kernel API*/

/*
 * Producer of data - prod_write() concates data into buffer written
 * into /dev/prod device. Buffer is limited to grow upto 32MB.
 */

ssize_t prod_write(struct file *filp, const char __user *buf, size_t count,
		loff_t *f_pos)
{
	int ret;
	int copied;
	pr_info("%s() : FIFO size = %d, count = %d\n", __func__,
			(int)kfifo_len(&fifo), (int)count);
	if (down_interruptible(&prod_sem))
		return -ERESTARTSYS;
	while ((int)kfifo_avail(&fifo) <= 0) { /* full */
		up(&prod_sem);
		if (filp->f_flags & O_NONBLOCK)
			return -EAGAIN;
		pr_info("%s() : \"%s\" going to sleep\n", __func__,
				current->comm);
		if (wait_event_interruptible(prod_que,\
					(((int)kfifo_avail(&fifo)) > 0))) {
			pr_info("%s() wait_event_interruptible() : signal: "
				"tell the fs layer to handle it\n", __func__);
			return -ERESTARTSYS;
			/* signal: inform the fs layer to handle it */
		}
		if (down_interruptible(&prod_sem))
			return -ERESTARTSYS;
		pr_info("%s() : \"%s\" waken from sleep\n", __func__,
				current->comm);
	}
	count = min((int)count, (int)kfifo_avail(&fifo));
	pr_info("%s() : \"%s\" data to copy = %li bytes\n",
		__func__, current->comm, (long)count);
	ret = kfifo_from_user(&fifo, buf, count, &copied);
	up(&prod_sem);
	if (ret < 0)
		return -EFAULT;
	pr_info("%s() : \"%s\" copied %d bytes.FIFO new SIZE = %d\n", __func__,
				current->comm, copied, (int)kfifo_len(&fifo));
	pr_info("%s() : \"%s\" waking up consumer processes\n", __func__,
				current->comm);
	wake_up_interruptible(&cons_que);
	return ret ? ret : copied;
}

/*
 * Consumer of data - cons_read() reads data from kernel buffer and copies to
 * userspace buffer. It clears the kernel buffer after copying complete buffer.
 */

ssize_t cons_read(struct file *filp, char __user *buf, size_t count,
		loff_t *f_pos)
{
	int ret;
	int copied;
	pr_info("%s() : FIFO size = %d, count = %d\n", __func__,
			(int)kfifo_len(&fifo), (int)count);
	if (down_interruptible(&cons_sem))
		return -ERESTARTSYS;

	while (kfifo_len(&fifo) <= 0) { /* nothing to read */
		up(&cons_sem); /* release the lock */
		if (filp->f_flags & O_NONBLOCK)
			return -EAGAIN;
		pr_info("%s () : \"%s\" going to sleep\n", __func__,
				current->comm);
		if (wait_event_interruptible(cons_que, kfifo_len(&fifo) > 0)) {
			pr_info("%s() wait_event_interruptible() : signal: "
				"tell the fs layer to handle it\n", __func__);
			return -ERESTARTSYS;
			/* signal: inform the fs layer to handle it */
		}
		if (down_interruptible(&cons_sem))
			return -ERESTARTSYS;
	}
	/* ok, data is there, return something */
	count = min((long)count, (long)kfifo_len(&fifo));
	pr_info("%s() : \"%s\" data to copy = %li bytes\n",
		__func__, current->comm, (long)count);
	ret = kfifo_to_user(&fifo, buf, count, &copied);
	up(&cons_sem);
	if (ret < 0)
		return -EFAULT;
	pr_info("%s() : \"%s\" read %li bytes. FIFO new Size = %d\n",
		__func__, current->comm, (long)count, (int)kfifo_len(&fifo));
	pr_info("%s() : \"%s\" waking up producer processes\n", __func__,
				current->comm);
	wake_up_interruptible(&prod_que);
	return copied;

}

static int prod_open(struct inode *inode, struct file *filp)
{
	printk(KERN_INFO "%s()\n", __func__);
	return 0;
}

static int prod_release(struct inode *inode, struct file *filp)
{
	printk(KERN_INFO "%s()\n", __func__);
	return 0;
}

static ssize_t prod_read(struct file *f, char __user *buf, size_t len,
			loff_t *off)
{
	printk(KERN_INFO "%s() Read at exit end of pipe at /dev/cons\n",
			__func__);
	return 0;
}

static int cons_open(struct inode *inode, struct file *filp)
{
	printk(KERN_INFO "%s()\n", __func__);
	return 0;
}


static int cons_release(struct inode *inode, struct file *filp)
{
	printk(KERN_INFO "%s()\n", __func__);
	return 0;
}

static ssize_t cons_write(struct file *f, const char __user *buf, size_t len,
				loff_t *off)
{
	char *t = "/dev/prod";
	printk(KERN_INFO "%s write at pipe entry point at %s\n", __func__, t);
	return len;
}

/*
 * Set up the cdev structure for a device.
 */
static int simple_setup_cdev(struct cdev *cdev, int minor,
		const struct file_operations *fops, const char *device_name)
{
	int err;
	int ret = 0;
	int devno = MKDEV(simple_major, minor);

	cdev_init(cdev, fops);
	cdev->owner = THIS_MODULE;
	cdev->ops = fops;

	if (device_create(cl, NULL, devno, NULL, device_name) == NULL) {
		printk(KERN_ERR "Error creating dev/%s\n", device_name);
		ret = -DEVCREATEERR;
		goto setup_out;
	}

	err = cdev_add(cdev, devno, 1);
	if (err) {
		printk(KERN_ERR "Error %d adding dev/%s\n", err, device_name);
		ret = -DEVADDERR;
		goto setup_out;
	}
setup_out:
	return ret;
}

static const struct file_operations prod_ops = {
	.owner   = THIS_MODULE,
	.open    = prod_open,
	.release = prod_release,
	.read = prod_read,
	.write = prod_write,
};

static const struct file_operations cons_ops = {
	.owner   = THIS_MODULE,
	.open    = cons_open,
	.release = cons_release,
	.read = cons_read,
	.write = cons_write,
};

static int simple_init(void)
{
	int result = 0;
	static dev_t dev_no;
	printk(KERN_INFO "%s()\n", __func__);
	result = alloc_chrdev_region(&dev_no, 0, 2, "simple");
	if (result < 0) {
		printk(KERN_WARNING "simple: unable to get major %d\n",
			simple_major);
		result = -EPERM;
		goto alloc_chedev_region_fail;
	}
	simple_major = MAJOR(dev_no);
	simple_minor = MINOR(dev_no);
	printk(KERN_INFO "%s() : Simple driver for 2 devices\n", __func__);
	printk(KERN_INFO "Major=%d,Minor=%d\n", simple_major, simple_minor);
	printk(KERN_INFO "MajoR=%d,Minor=%d\n", simple_major, (simple_minor+1));

	cl = class_create(THIS_MODULE, "chardrv");
	if (cl == NULL) {
		result = -EPERM;
		goto class_create_fail;
	}

	/* Now set up two cdevs. */
	result = simple_setup_cdev(simple_cdev, 0, &prod_ops,
			"prod");
	if (result < 0) {
		if (result == -DEVADDERR)
			goto dev_add_err_0;
		else
			goto dev_create_err_0;
	}

	result = simple_setup_cdev(simple_cdev + 1, 1, &cons_ops,
			"cons");
	if (result < 0) {
		if (result == -DEVADDERR)
			goto dev_add_err_1;
		else
			goto dev_create_err_1;
	}
	/*
	 * memory for data
	 */
	INIT_KFIFO(fifo);
	sema_init(&prod_sem, 1);
	sema_init(&cons_sem, 1);
	init_waitqueue_head(&prod_que);
	init_waitqueue_head(&cons_que);
	goto init_success;

dev_add_err_1:
	cdev_del(simple_cdev + 1);
dev_create_err_1:
dev_add_err_0:
	cdev_del(simple_cdev);
dev_create_err_0:
class_create_fail:
	unregister_chrdev_region(MKDEV(simple_major, simple_minor), 2);
alloc_chedev_region_fail:
	result  = -1;
init_success:
	return result;
}

static void simple_cleanup(void)
{
	cdev_del(simple_cdev);
	cdev_del(simple_cdev + 1);
	device_destroy(cl, MKDEV(simple_major, simple_minor));
	device_destroy(cl, MKDEV(simple_major, (simple_minor + 1)));
	class_destroy(cl);
	unregister_chrdev_region(MKDEV(simple_major, simple_minor), 2);
	printk(KERN_INFO "%s() : Driver cleanup complete\n", __func__);
}

module_init(simple_init);
module_exit(simple_cleanup);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("SUNIL SHAHU & DEVARSH THAKKAR");
