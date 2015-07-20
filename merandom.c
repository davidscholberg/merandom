/* TODO: enable multiple simultaneous reads */
/* TODO: enable write operation on device file to set seed value? */
/* TODO: find out where file_operations interfaces are documented */
/* TODO: look into platform portability:
 * http://kernelnewbies.org/WritingPortableDrivers */

#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>

static void merandom_fill_rb(char *, size_t);
static uint64_t merandom_prng(void);
static void merandom_seed(void);
static int merandom_init(void) __init;
static void merandom_exit(void) __exit;
static int merandom_open(struct inode *, struct file *);
static int merandom_release(struct inode *, struct file *);
static ssize_t merandom_read(struct file *, char *, size_t, loff_t *);
static ssize_t merandom_write(struct file *, const char *, size_t, loff_t *);

#define MERANDOM_DEV_NAME "merandom"
#define MERANDOM_BUF_LEN (128 * sizeof(uint64_t))
#define MERANDOM_STATE_BUF_LEN 16

static int device_open;       /* is device open? */

uint64_t rand_state[MERANDOM_STATE_BUF_LEN];
int rand_state_p;

static char rb[MERANDOM_BUF_LEN]; /* buffer with random data */
static char *rb_ptr;              /* pointer to current rb position */

static const struct file_operations fops = {
	.owner = THIS_MODULE,
	.read = merandom_read,
	.write = merandom_write,
	.open = merandom_open,
	.release = merandom_release
};

static struct miscdevice merandom_miscdev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = MERANDOM_DEV_NAME,
	.fops = &fops,
	.mode = S_IRUGO,
};

/*
 * Fill buffer with random data
 */
static void merandom_fill_rb(char *rb_ptr, size_t len)
{
	uint64_t rn;

	while (len) {
		rn = merandom_prng();
		/* TODO: make this a for loop so that buf len doesn't matter */
		*((uint64_t *) rb_ptr) = rn;

		rb_ptr += sizeof(uint64_t);
		len -= sizeof(uint64_t);
	}
}

/*
 * PRNG taken from https://en.wikipedia.org/wiki/Xorshift#Xorshift.2A
 * This is an implementation of xorshift1024*
 */
static uint64_t merandom_prng(void)
{
	uint64_t s0 = rand_state[rand_state_p];
	uint64_t s1 = rand_state[rand_state_p = (rand_state_p + 1) & 15];

	s1 ^= s1 << 31;
	s1 ^= s1 >> 11;
	s0 ^= s0 >> 30;
	return (rand_state[rand_state_p] = s0 ^ s1) * 1181783497276652981ULL;
}

static void merandom_seed(void)
{
	uint64_t i;

	for (i = 0; i < MERANDOM_STATE_BUF_LEN; i++)
		rand_state[i] = i + 1;

	rand_state_p = i + 1;
}

static int merandom_open(struct inode *inode, struct file *file)
{
	if (device_open)
		return -EBUSY;

	device_open++;

	try_module_get(THIS_MODULE);

	return 0;
}

static int merandom_release(struct inode *inode, struct file *file)
{
	device_open--;

	/*
	 * Decrement the usage count, or else once you opened the file, you'll
	 * never get get rid of the module.
	 */
	module_put(THIS_MODULE);

	return 0;
}

static ssize_t merandom_read(struct file *filp,	/* see include/linux/fs.h   */
			   char __user *buf,	/* buffer to fill with data */
			   size_t len,		/* length of the buffer     */
			   loff_t *off)
{
	ssize_t bytes_read = len; /* always output len bytes */
	ssize_t bytes_to_copy = 0;

	while (len) {
		/* if we run out of random data, get more */
		if (rb_ptr == rb + sizeof(rb)) {
			merandom_fill_rb(rb, sizeof(rb));
			rb_ptr = rb;
		}

		bytes_to_copy = min(len, sizeof(rb) - (rb_ptr - rb));
		if (copy_to_user(buf, rb_ptr, bytes_to_copy))
			return -EFAULT;

		rb_ptr += bytes_to_copy;
		buf += bytes_to_copy;
		len -= bytes_to_copy;
	}

	return bytes_read;
}

static ssize_t
merandom_write(struct file *filp, const char *buff, size_t len, loff_t *off)
{
	pr_alert("%s: write operation on /dev/%s not supported\n",
			MERANDOM_DEV_NAME,
			MERANDOM_DEV_NAME);
	return -EINVAL;
}

static int __init merandom_init(void)
{
	int ret;

	ret = misc_register(&merandom_miscdev);
	if (ret) {
		pr_err("%s: misc_register failed: %d\n", MERANDOM_DEV_NAME,
				ret);
		return ret;
	}

	pr_info("%s: registered\n", MERANDOM_DEV_NAME);
	pr_info("%s: created device file /dev/%s\n",
			MERANDOM_DEV_NAME, MERANDOM_DEV_NAME);

	device_open = 0;

	/* TODO: seed with actual random data */
	merandom_seed();

	/* fill buffer with new random data and set pointer */
	merandom_fill_rb(rb, sizeof(rb));
	rb_ptr = rb;

	return 0;
}

static void __exit merandom_exit(void)
{
	int ret;

	ret = misc_deregister(&merandom_miscdev);
	if (ret) {
		pr_err("%s: misc_deregister failed: %d\n", MERANDOM_DEV_NAME,
				ret);
	}
	else {
		pr_info("%s: unregistered\n", MERANDOM_DEV_NAME);
		pr_info("%s: deleted /dev/%s\n",
				MERANDOM_DEV_NAME,
				MERANDOM_DEV_NAME);
	}
}

module_init(merandom_init);
module_exit(merandom_exit);

MODULE_AUTHOR("David Scholberg <recombinant.vector@gmail.com>");
MODULE_DESCRIPTION("Module for a /dev/urandom-like character device");
MODULE_LICENSE("GPL");
