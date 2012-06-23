#include <linux/sched.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/time.h>
#include <linux/vmalloc.h>
#include <linux/kallsyms.h>

#include <asm/ioctls.h>

/*struct uhook*/
#define   MAX_ARGV_LEN	 512			/*The length of the buffer used to staorge argv*/
#define   UHOOK_STA_LEN	 32			/*The length of the buffer used to staorge argv*/


#define   UHOOKCMD_QUERY_FUNC	1		/*CMD used to query if there is a symbal in kernel*/
#define   UHOOKCMD_QUERY_VAL	2		/*CMD used to query the value of a argument in kernel*/
#define   UHOOKCMD_RUN		3		/*CMD used to run the func in kernel*/



struct uhook{
	char 		fun_name[KSYM_NAME_LEN];/*function name called from userspace*/
	char 		argv[MAX_ARGV_LEN];	/*argv of function*/
	int 		argc;			/*number of arg*/
	int		ret;			/*return value*/
	char		status[UHOOK_STA_LEN];	/*status of kernel func run*/
	unsigned long	addr;			/*Address of the kernel symbal*/
};

struct uhook_desc{
	struct miscdevice	misc;		/*The misc device*/
	struct mutex 		mutex;		/*mutex used to protect*/
};

static struct uhook_desc *uhook_global;

int uhook_unlock(void)
{
	printk(KERN_INFO"Unlock mutex\n");
	mutex_unlock(&uhook_global->mutex);
	return -1;
}

int  uhook_test(void)
{
	printk(">>>>>>>>>>uhook test sucessfully<<<<<<<<<<<<<<<<<<<\n");
	return -1;
}
EXPORT_SYMBOL_GPL(uhook_test);
EXPORT_SYMBOL_GPL(uhook_unlock);


static ssize_t uhook_read(struct file *file, char __user *buf,
			   size_t count, loff_t *pos)
{
	return 0;
}



static ssize_t uhook_aio_write(struct kiocb *iocb, const struct iovec *iov,
			 unsigned long nr_segs, loff_t ppos)
{
	return 0;
}



static int uhook_open(struct inode *inode, struct file *file)
{

	file->private_data = uhook_global;
	printk(KERN_INFO"Open uhook device success\n");
	return 0;
}

static int uhook_release(struct inode *ignored, struct file *file)
{
	return 0;
}


static long uhook_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct uhook_desc *desc = file->private_data;
	struct uhook *tmp = (struct uhook *)arg;

	/*Status of kernel function run*/
	char *exsit = "exsit";
	char *noexsit = "no-exsit";
	char *success = "success";
	char *fail = "fail";

	struct uhook	uhook;
	unsigned long addr;
	int ret = 0;
	int val = 0;

	memset(&uhook, 0, sizeof(struct uhook));
	mutex_lock(&desc->mutex);

	switch (cmd) {
	case UHOOKCMD_QUERY_FUNC:
		if (copy_from_user(&uhook, (struct uhook __user *)arg, sizeof(struct uhook))) {
			mutex_unlock(&desc->mutex);
			return -EFAULT;
		}
		addr = kallsyms_lookup_name(uhook.fun_name);
		if (addr) {
			if (copy_to_user(&tmp->addr, &addr, sizeof(unsigned long))) {
				mutex_unlock(&desc->mutex);
				return -EFAULT;
			}
			ret = 0;
			if (copy_to_user(&tmp->ret, &ret, sizeof(unsigned long))) {
				mutex_unlock(&desc->mutex);
				return -EFAULT;
			}
			if (copy_to_user(tmp->status, exsit, strlen(exsit))) {
				mutex_unlock(&desc->mutex);
				return -EFAULT;
			}
			mutex_unlock(&desc->mutex);

		} else {
			ret = -1;
			if (copy_to_user(&tmp->addr, &ret, sizeof(unsigned long))) {
				mutex_unlock(&desc->mutex);
				return -EFAULT;
			}
			if (copy_to_user(&tmp->ret, &ret, sizeof(int))) {
				mutex_unlock(&desc->mutex);
				return -EFAULT;
			}
			if (copy_to_user(tmp->status, noexsit, strlen(noexsit))) {
				mutex_unlock(&desc->mutex);
				return -EFAULT;
			}
			mutex_unlock(&desc->mutex);
		}
		break;
	case UHOOKCMD_QUERY_VAL:
	/*TODO:
	 * this version of uhook can just return the int value(4 byte for 32 bit system)
	 * from kernel space to user space. And if the value is -1, how to deal it?*/
		if (copy_from_user(&uhook, (struct uhook __user *)arg, sizeof(struct uhook))) {
			mutex_unlock(&desc->mutex);
			return -EFAULT;
		}
		addr = kallsyms_lookup_name(uhook.fun_name);
		if (addr) {
			val = *(int *)addr;
			if (copy_to_user(&tmp->addr, &addr, sizeof(unsigned long))) {
				mutex_unlock(&desc->mutex);
				return -EFAULT;
			}
			if (copy_to_user(&tmp->ret, &val, sizeof(int))) {
				mutex_unlock(&desc->mutex);
				return -EFAULT;
			}
			if (copy_to_user(tmp->status, success, strlen(success))) {
				mutex_unlock(&desc->mutex);
				return -EFAULT;
			}
			mutex_unlock(&desc->mutex);
		} else {
			ret = -1;
			if (copy_to_user(&tmp->addr, &ret, sizeof(unsigned long))) {
				mutex_unlock(&desc->mutex);
				return -EFAULT;
			}
			if (copy_to_user(&tmp->ret, &ret, sizeof(int))) {
				mutex_unlock(&desc->mutex);
				return -EFAULT;
			}
			if (copy_to_user(tmp->status, fail, strlen(fail))) {
				mutex_unlock(&desc->mutex);
				return -EFAULT;
			}
			mutex_unlock(&desc->mutex);
		}
		break;
	case UHOOKCMD_RUN:
	/*This is the point. Call kernel space function from user space
	 * TODO:
	 * This version of uhook can just support function type like:
	 * int func(void)*/
		if (copy_from_user(&uhook, (struct uhook __user *)arg, sizeof(struct uhook))) {
			mutex_unlock(&desc->mutex);
			return -EFAULT;
		}
		addr = kallsyms_lookup_name(uhook.fun_name);
		if (addr) {
			int (*func)(void);
			func = (int (*)(void))addr;
			ret = func();

			if (copy_to_user(&tmp->addr, &addr, sizeof(unsigned long))) {
				mutex_unlock(&desc->mutex);
				return -EFAULT;
			}
			if (copy_to_user(&tmp->ret, &ret, sizeof(int))) {
				mutex_unlock(&desc->mutex);
				return -EFAULT;
			}
			if (copy_to_user(tmp->status, success, strlen(success))) {
				mutex_unlock(&desc->mutex);
				return -EFAULT;
			}
			mutex_unlock(&desc->mutex);
		} else {
			ret = -1;

			if (copy_to_user(&tmp->addr, &ret, sizeof(unsigned long))) {
				mutex_unlock(&desc->mutex);
				return -EFAULT;
			}
			if (copy_to_user(&tmp->ret, &ret, sizeof(int))) {
				mutex_unlock(&desc->mutex);
				return -EFAULT;
			}
			if (copy_to_user(tmp->status, noexsit, strlen(noexsit))) {
				mutex_unlock(&desc->mutex);
				return -EFAULT;
			}
			mutex_unlock(&desc->mutex);
		}
		break;
	default:
		printk(KERN_ERR"Unknown cmd\n");
	}

	mutex_unlock(&desc->mutex);

	return ret;
}

static const struct file_operations uhook_fops = {
	.owner = THIS_MODULE,
	.read = uhook_read,
	.aio_write = uhook_aio_write,
	.unlocked_ioctl = uhook_ioctl,
	.compat_ioctl = uhook_ioctl,
	.open = uhook_open,
	.release = uhook_release,
};

static int __init uhook_init(char *uhook_name)
{
	uhook_global = kzalloc(sizeof(struct uhook_desc), GFP_KERNEL);
	int ret = 0;
	if (!uhook_global)
		return -ENOMEM;

	/*Do some initilization*/
	mutex_init(&uhook_global->mutex);

	uhook_global->misc.minor = MISC_DYNAMIC_MINOR;
	uhook_global->misc.name = kstrdup(uhook_name, GFP_KERNEL);
	if (uhook_global->misc.name == NULL) {
		ret = -ENOMEM;
		goto out_free;
	}

	uhook_global->misc.fops = &uhook_fops;
	uhook_global->misc.parent = NULL;

	/*Register the misc device for uhook */
	ret = misc_register(&uhook_global->misc);
	if (unlikely(ret)) {
		printk(KERN_ERR "uhook: failed to register misc "
		       "device for log '%s'!\n", uhook_global->misc.name);
		goto out_free;
	}

	printk(KERN_INFO "uhook: created:'%s'\n",
	       uhook_global->misc.name);

	return 0;

out_free:
	kfree(uhook_global);

	return ret;
}
static int __init uhook_dev_init(void)
{
	if (uhook_init("uhook")){
		printk(KERN_ERR"uhook init failed\n");
		return -1;
	}
	printk(KERN_INFO"uhook init success\n");
	return 0;
}

static void __exit uhook_dev_deinit(void)
{
	misc_deregister(&uhook_global->misc);
	kfree(uhook_global);
}
MODULE_LICENSE("GPL");
module_init(uhook_dev_init);
module_exit(uhook_dev_deinit);