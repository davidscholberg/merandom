// TODO: use mknod system call within module: http://stackoverflow.com/questions/5970595/create-a-device-node-in-code
// TODO: enable write operation on device file to set seed value?
// TODO: find out where file_operations interfaces are documented
// TODO: look into platform portability: http://kernelnewbies.org/WritingPortableDrivers

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <asm/uaccess.h>

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
#define MERANDOM_BUF_LEN 16 * sizeof (uint64_t)
#define MERANDOM_STATE_BUF_LEN 16

static int major_num;
static int device_open = 0;       // is device open?
//static unsigned char seed;
//static unsigned char rc;          // current random character

uint64_t rand_state[MERANDOM_STATE_BUF_LEN];
int rand_state_p;

static char rb[MERANDOM_BUF_LEN]; // buffer with random data
static char *rb_ptr;              // pointer to current rb position

static struct file_operations fops = {
	.owner = THIS_MODULE,
	.read = merandom_read,
	.write = merandom_write,
	.open = merandom_open,
	.release = merandom_release
};

/*
 * Fill buffer with random data
 */
static void merandom_fill_rb(char *rb_ptr, size_t len)
{
	uint64_t rn;

	while (len) {
		//rc = (rc * 7) % 11;
		rn = merandom_prng();
		// TODO: make this a for loop so that buf len doesn't matter
		*((uint64_t*) rb_ptr) = rn;

		rb_ptr += sizeof (uint64_t);
		len -= sizeof (uint64_t);
	}
}

/*
 * PRNG taken from https://en.wikipedia.org/wiki/Xorshift#Xorshift.2A
 * This is an implementation of xorshift1024*
 */
static uint64_t merandom_prng(void) {
	uint64_t s0 = rand_state[rand_state_p];
	uint64_t s1 = rand_state[rand_state_p = (rand_state_p + 1) & 15];
	s1 ^= s1 << 31;
	s1 ^= s1 >> 11;
	s0 ^= s0 >> 30;
	return (rand_state[rand_state_p] = s0 ^ s1) * 1181783497276652981ULL; 
}

static void merandom_seed(void) {
	uint64_t i;
	for (i = 0; i < MERANDOM_STATE_BUF_LEN; i++) {
		rand_state[i] = i + 1;
	}

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
			   char *buf,		/* buffer to fill with data */
			   size_t len,		/* length of the buffer     */
			   loff_t * off)
{
	ssize_t bytes_read = len; // always output len bytes

	while (len) {
		// if we run out of random data, get more
		if (rb_ptr == rb + sizeof rb) {
			merandom_fill_rb(rb, sizeof rb);
			rb_ptr = rb;
		}
		/* 
		 * The buffer is in the user data segment, not the kernel 
		 * segment so "*" assignment won't work.  We have to use 
		 * put_user which copies data from the kernel data segment to
		 * the user data segment. 
		 */
		put_user(*rb_ptr, buf);

		rb_ptr++;
		buf++;
		len--;
	}

	return bytes_read;
}

static ssize_t
merandom_write(struct file *filp, const char *buff, size_t len, loff_t * off)
{
	printk(KERN_ALERT "%s: write operation on /dev/%s not supported\n",
			MERANDOM_DEV_NAME,
			MERANDOM_DEV_NAME);
	return -EINVAL;
}

static int __init merandom_init(void)
{
	major_num = register_chrdev(0, MERANDOM_DEV_NAME, &fops);
	if (major_num < 0) {
		printk(KERN_ALERT "%s: registration failed: %d\n", MERANDOM_DEV_NAME, major_num);
		return major_num;
	}

	printk(KERN_INFO "%s: registered\n", MERANDOM_DEV_NAME);
	printk(KERN_INFO "%s: create device file with: mknod /dev/%s c %d 0\n",
			MERANDOM_DEV_NAME,
			MERANDOM_DEV_NAME,
			major_num);

	// TODO: seed with actual random value
	merandom_seed();
	//seed = 34;
	//rc = seed;

	//printk(KERN_INFO "%s: seeded with value %u\n", MERANDOM_DEV_NAME, seed);

	// fill buffer with new random data and set pointer
	merandom_fill_rb(rb, sizeof rb);
	rb_ptr = rb;

	return 0;
}

static void __exit merandom_exit(void)
{
	unregister_chrdev(major_num, MERANDOM_DEV_NAME);

	printk(KERN_INFO "%s: unregistered\n", MERANDOM_DEV_NAME);
	printk(KERN_INFO "%s: /dev/%s is no longer needed\n",
			MERANDOM_DEV_NAME,
			MERANDOM_DEV_NAME);
}

module_init(merandom_init);
module_exit(merandom_exit);

MODULE_AUTHOR("David Scholberg <recombinant.vector@gmail.com>");
MODULE_DESCRIPTION("Module for a /dev/urandom-like character device");
MODULE_LICENSE("GPL");
