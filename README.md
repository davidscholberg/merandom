## merandom

merandom is a Linux kernel module that implements a `/dev/urandom`-like character device file. The module creates the character device `/dev/merandom` which, when read from, outputs a string of "random" bytes using a pseudo random number generator (PRNG).

This project is intended to be used solely for educational purposes. See the [Why](#why) and [Disclaimer](#disclaimer) sections.

### Build

To build the module, you'll need a working Linux installation and the Linux kernel headers for the running kernel. You can either use your distribution's Linux headers package, or grab the [Linux kernel source](https://www.kernel.org/) directly. You'll also need `gcc` and `make`.

Assuming you have the Linux headers and build tools installed, you can build the merandom module with the following commands:

```
git clone https://github.com/davidscholberg/merandom.git
cd merandom
KDIR=/lib/modules/$(uname -r)/build make
```

If you grabbed the kernel source manually, then you need to change the `KDIR` variable to point to the directory where you extracted/cloned the kernel source.

### Load/Unload

Assuming the build succeeded, you can load the module with the following command:

```
sudo insmod ./merandom.ko
```

The kernel log should indicate whether or not the module was successfully loaded.

The module can be unloaded with this command:

```
sudo rmmod merandom
```

### Usage

If the module is successfully loaded, the `/dev/merandom` device file is automatically created. You can test it with the `dd` utility as follows:

```
dd if=/dev/merandom bs=16 count=1 2>/dev/null | hexdump -C
```

The above command reads 16 bytes from `/dev/merandom` and outputs it in hex format using `hexdump`. If you run this command repeatedly, you should get random output every time.

Note that currently the seed value for this module's pseudo random number generator is always set to the same value when the module is loaded, which means that you'll get the same string of "random" bytes every time you unload and reload the module. One of the TODO items for this project is to use a good source of entropy to seed the PRNG.

### Why

I started working on this module with the intention of learning about Linux kernel programming. There are lots of great resources online for this topic, but many of them are several years old. Even though these older resourcs introduce kernel constructs that can still be used in recent kernels, there are often newer, prefered ways of doing things due to the constant forward momentum of Linux kernel development.

This module was originally coded as I was working my way through [The Linux Kernel Module Programming Guide](http://www.tldp.org/LDP/lkmpg/2.6/html/lkmpg.html). As I learned more about the more recent developments in the Linux kernel, I came back to this module and updated it with what I'd learned.

My goal is to continue to update this module as I learn about relevant recent kernel developments or just better ways of doing things, and to thoroughly document everything that may be non-obvious. Hopefully, people in the process of learning about Linux kernel development can use this module as another resource to supplement their learning.

### Disclaimer

The PRNG that this module implements is not intended to be cryptographically secure in any way. If you need a source of cryptographically secure random bytes on a Linux system, then use `/dev/random`. For anything else requiring random bytes where cryptographic security is not a requirement, use `/dev/urandom`. If you want to learn about Linux kernel module programming and have a little fun, then use `/dev/merandom` :)
