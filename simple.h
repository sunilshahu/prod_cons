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
#include <linux/kfifo.h>
#include <linux/semaphore.h>
#include <linux/mutex.h>
#include <linux/wait.h>
#include  <linux/sched.h>

#define MAX_SIMPLE_DEV 2

#define DEVCREATEERR	1001
#define DEVADDERR	1002

#define KB		1024
#define MB		(1024*KB)
#define FIFO_SIZE	(32*MB)
