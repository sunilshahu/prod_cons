/*
 * my_device_0, it takes data and stores
 * in a linked list
 * my_device_1, it takes data from linked
 * list and returns to user
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>

#include <linux/kernel.h>   /* printk() */
#include <linux/slab.h>   /* kmalloc() */
#include <linux/fs.h>       /* everything... */
#include <linux/errno.h>    /* error codes */
#include <linux/types.h>    /* size_t */
#include <linux/mm.h>
#include <linux/kdev_t.h>
#include <asm/page.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/device.h>
#include <linux/string.h>
#include "simple.h"
static struct class *cl;
static int simple_major;
static int simple_minor;
int my_device_no;
char my_device_string[20];
static dev_t dev;

struct scull_dev *scull_private_data;

int scull_trim(struct scull_dev *dev)
{
	struct scull_qset *next, *dptr;
	int qset = dev->qset;   /* "dev" is not-null */
	int i, j;
	j = 0;

	printk(KERN_INFO "%s() : cleaning up BUFFER..\n", __func__);
	for (dptr = dev->data; dptr; dptr = next) { /* all the list items */
		if (dptr->data) {
			for (i = 0; i < qset; i++)
				kfree(dptr->data[i]);
			printk(KERN_INFO "%s() : freed no of quantum = %d\n",
					__func__, i);
			kfree(dptr->data);
			dptr->data = NULL;
		}
		next = dptr->next;
		kfree(dptr);
		j++;
	}
	printk(KERN_INFO "%s() : freed no of qset = %d\n", __func__, j);
	dev->size = 0;
	dev->data = NULL;
	return 0;
}

struct scull_qset *scull_follow(struct scull_dev *dev, int n)
{
	struct scull_qset *qs = dev->data;

	/* Allocate first qset explicitly if need be */
	if (!qs) {
		qs = dev->data = kmalloc(sizeof(struct scull_qset), GFP_KERNEL);
		if (qs == NULL)
			return NULL;  /* Never mind */
		memset(qs, 0, sizeof(struct scull_qset));
	}

	/* Then follow the list */
	while (n--) {
		if (!qs->next) {
			qs->next = kmalloc(sizeof(struct scull_qset),\
					GFP_KERNEL);
			if (qs->next == NULL)
				return NULL;  /* Never mind */
			memset(qs->next, 0, sizeof(struct scull_qset));
		}
		qs = qs->next;
		continue;
	}
	return qs;
}


ssize_t scull_write(struct file *filp, const char __user *buf, size_t count,
		loff_t *f_pos)
{
	struct scull_qset *dptr;
	int quantum = scull_private_data->quantum;
	int qset = scull_private_data->qset;
	int itemsize = quantum * qset;
	int item, s_pos, q_pos, rest;
	ssize_t retval = -ENOMEM; /* value used in "goto out" statements */
	unsigned long int max_data = SCULL_MAX_DATA;

	if (scull_private_data->size >= max_data) {
		printk(KERN_ERR "32 MB Data buffer full or data is too large.");
		printk(KERN_ERR "Please read and clear.\n");
		goto out;
	}
	/* find listitem, qset index and offset in the quantum */

	item = (long)scull_private_data->size / itemsize;
	rest = (long)scull_private_data->size % itemsize;
	s_pos = rest / quantum; q_pos = rest % quantum;

	/* follow the list up to the right position */
	dptr = scull_follow(scull_private_data, item);
	if (dptr == NULL)
		goto out;
	if (!dptr->data) {
		dptr->data = kmalloc(qset * sizeof(char *), GFP_KERNEL);
		if (!dptr->data)
			goto out;
		memset(dptr->data, 0, qset * sizeof(char *));
	}
	if (!dptr->data[s_pos]) {
		/*goto perticular quantum and create if not present */
		dptr->data[s_pos] = kmalloc(quantum, GFP_KERNEL);
		if (!dptr->data[s_pos])
			goto out;
	}
	/* write only up to the end of this quantum */
	if (count > quantum - q_pos)
		count = quantum - q_pos;
	/* in qset(data)->quantum(s_pos)->byteposition(q_post) wrte data */
	if (copy_from_user(dptr->data[s_pos]+q_pos, buf, count)) {
		retval = -EFAULT;
		goto out;
	}
	*f_pos += count;
	scull_private_data->size += count;
	retval = count;

out:
	printk(KERN_INFO "%s(): DATA WRITE = %d bytes, BUFFER SIZE = %ld\n",
			__func__, (int)retval, scull_private_data->size);
	return retval;
}
ssize_t scull_read(struct file *filp, char __user *buf, size_t count,
		loff_t *f_pos)
{
	struct scull_qset *dptr;	/* the first listitem */
	int quantum = scull_private_data->quantum;
	int qset = scull_private_data->qset;
	int itemsize = quantum * qset; /* how many bytes in the listitem */
	int item, s_pos, q_pos, rest;
	ssize_t retval = 0;

	if (*f_pos >= scull_private_data->size) {
		scull_trim(scull_private_data);
		goto out;
	}
	if (*f_pos + count > scull_private_data->size)
		count = scull_private_data->size - *f_pos;

	/* find listitem,qset index,and offset in the quantum*/
	item = (long)*f_pos / itemsize;
	rest = (long)*f_pos % itemsize;
	s_pos = rest / quantum; q_pos = rest % quantum;

	/* follow the list up to the right position (defined elsewhere) */
	dptr = scull_follow(scull_private_data, item);

	if (dptr == NULL || !dptr->data || !dptr->data[s_pos])
		goto out; /* don't fill holes */

	/* read only up to the end of this quantum */
	if (count > quantum - q_pos)
		count = quantum - q_pos;

	if (copy_to_user(buf, dptr->data[s_pos] + q_pos, count)) {
		retval = -EFAULT;
		goto out;
	}
	*f_pos += count;
	retval = count;

out:
	return retval;
}

static int simple_open_0(struct inode *inode, struct file *filp)
{
	printk(KERN_INFO "%s() : SIMPLE OPEN\n", __func__);
	return 0;
}


static int simple_release_0(struct inode *inode, struct file *filp)
{
	printk(KERN_INFO "%s() : SIMPLE close\n", __func__);
	return 0;
}

static ssize_t my_read_0(struct file *f, char __user *buf, size_t len,
			loff_t *off)
{
	printk(KERN_INFO "%s() Read at exit end of pipe at /dev/my_device_1\n",
			__func__);
	return 0;
}

static int simple_open_1(struct inode *inode, struct file *filp)
{
	printk(KERN_INFO "%s() : SIMPLE OPEN\n", __func__);
	return 0;
}


static int simple_release_1(struct inode *inode, struct file *filp)
{
	printk(KERN_INFO "%s() : SIMPLE close\n", __func__);
	return 0;
}

static ssize_t my_write_1(struct file *f, const char __user *buf, size_t len,
				loff_t *off)
{
	char *t = "/dev/my_device_0";
	printk(KERN_INFO "%s write at pipe entry point at %s\n", __func__, t);
	return len;
}

/*
 * Set up the cdev structure for a device.
 */
static int simple_setup_cdev(struct cdev *dev, int minor,
		const struct file_operations *fops)
{
	int err;
	int ret = 0;
	int devno = MKDEV(simple_major, minor);

	cdev_init(dev, fops);
	dev->owner = THIS_MODULE;
	dev->ops = fops;
	snprintf(my_device_string, sizeof(my_device_string), "my_device_%d",
			my_device_no++);

	if (device_create(cl, NULL, devno, NULL, my_device_string) == NULL) {
		printk(KERN_ERR "Error creating simple%d", minor);
		ret = -DEVCREATEERR;
		goto setup_out;
	}

	err = cdev_add(dev, devno, 1);
	if (err) {
		printk(KERN_ERR "Error %d adding simple%d", err, minor);
		ret = -DEVADDERR;
		goto setup_out;
	}
setup_out:
	return ret;
}


static const struct file_operations simple_device_0_ops = {
	.owner   = THIS_MODULE,
	.open    = simple_open_0,
	.release = simple_release_0,
	.read = my_read_0,
	.write = scull_write,
};

static const struct file_operations simple_device_1_ops = {
	.owner   = THIS_MODULE,
	.open    = simple_open_1,
	.release = simple_release_1,
	.read = scull_read,
	.write = my_write_1,
};

#define MAX_SIMPLE_DEV 2

static struct cdev SimpleDevs[MAX_SIMPLE_DEV];

static int simple_init(void)
{
	int result = 0;
	printk(KERN_INFO "%s()\n", __func__);

	scull_private_data = kmalloc(sizeof(struct scull_dev), GFP_KERNEL);
	if (!scull_private_data) {
		result = -ENOMEM;
		printk(KERN_ERR "simple: unable to kmalloc PRIVATE DATA\n");
		goto kmalloc_fail;
	}
	memset(scull_private_data, '\0', sizeof(struct scull_dev));
	scull_private_data->quantum = SCULL_QUANTUM;
	scull_private_data->qset = SCULL_QSET;
	scull_private_data->size = 0;
	scull_private_data->data = NULL;

	/* Figure out our device number. */
	result = alloc_chrdev_region(&dev, 0, 2, "simple");
	if (result < 0) {
		printk(KERN_WARNING "simple: unable to get major %d\n",
			simple_major);
		result = -EPERM;
		goto alloc_chedev_region_fail;
	}
	simple_major = MAJOR(dev);
	simple_minor = MINOR(dev);
	printk(KERN_INFO "%s() : Simple driver for 2 devices\n", __func__);
	printk(KERN_INFO "Major=%d,Minor=%d\n", simple_major, simple_minor);
	printk(KERN_INFO "MajoR=%d,Minor=%d\n", simple_major, (simple_minor+1));

	cl = class_create(THIS_MODULE, "chardrv");
	if (cl == NULL) {
		result = -EPERM;
		goto class_create_fail;
	}

	/* Now set up two cdevs. */
	result = simple_setup_cdev(SimpleDevs, 0, &simple_device_0_ops);
	if (result < 0) {
		if (result == -DEVADDERR)
			goto dev_add_err_0;
		else
			goto dev_create_err_0;
	}

	result = simple_setup_cdev(SimpleDevs + 1, 1, &simple_device_1_ops);
	if (result < 0) {
		if (result == -DEVADDERR)
			goto dev_add_err_1;
		else
			goto dev_create_err_1;
	}
	goto init_success;

dev_add_err_1:
	cdev_del(SimpleDevs + 1);
dev_create_err_1:
dev_add_err_0:
	cdev_del(SimpleDevs);
dev_create_err_0:
class_create_fail:
	unregister_chrdev_region(MKDEV(simple_major, simple_minor), 2);
alloc_chedev_region_fail:
	kfree(scull_private_data);
kmalloc_fail:
	result  = -1;
init_success:
	return result;
}


static void simple_cleanup(void)
{
	cdev_del(SimpleDevs);
	cdev_del(SimpleDevs + 1);
	device_destroy(cl, MKDEV(simple_major, simple_minor));
	device_destroy(cl, MKDEV(simple_major, (simple_minor + 1)));
	class_destroy(cl);
	unregister_chrdev_region(MKDEV(simple_major, simple_minor), 2);
	scull_trim(scull_private_data);
	printk(KERN_INFO "%s() : Driver cleanup complete\n", __func__);
}

module_init(simple_init);
module_exit(simple_cleanup);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("SUNIL SHAHU");
