/*
 * Device driver for the VGA LED Emulator
 *
 * A Platform device implemented using the misc subsystem
 *
 * Stephen A. Edwards
 * Columbia University
 *
 * References:
 * Linux source: Documentation/driver-model/platform.txt
 *               drivers/misc/arm-charlcd.c
 * http://www.linuxforu.com/tag/linux-device-drivers/
 * http://free-electrons.com/docs/
 *
 * "make" to build
 * insmod vga_led.ko
 *
 * Check code style with
 * checkpatch.pl --file --no-tree vga_led.c
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include "vga_fb.h”

#define DRIVER_NAME "vga_fb”

/*
 * Information about our device
 */
struct vga_fb_dev {
	struct resource res; /* Resource: our registers */
	void __iomem *virtbase; /* Where registers can be accessed in memory */
	u32 colors[VGA_PIXELS];
} dev;

/*
 * Write segments of a single digit
 * Assumes digit is in range and the device information has been set up
 */
static void write_digit(int pixel, u32 colors)
{
	
	iowrite32(colors, dev.virtbase + pixel);
	dev.colors[pixel] = colors;
}

/*
 * Handle ioctl() calls from userspace:
 * Read or write the segments on single digits.
 * Note extensive error checking of arguments
 */
static long vga_fb_ioctl(struct file *f, unsigned int cmd, unsigned long arg)
{
	vga_fb_arg_t vla;

	switch (cmd) {
	case VGA_FB_WRITE_DIGIT:
		if (copy_from_user(&vla, (vga_fb_arg_t *) arg,
				   sizeof(vga_fb_arg_t)))
			return -EACCES;
		if (vla.pixel > 30719)
			return -EINVAL;
		write_digit(vla.pixel, vla.colors);
		break;

	case VGA_LED_READ_DIGIT:
		if (copy_from_user(&vla, (vga_fb_arg_t *) arg,
				   sizeof(vga_fb_arg_t)))
			return -EACCES;
		if (vla.pixel > 30719)
			return -EINVAL;
		vla.colors = dev.colors[vla.pixel];
		if (copy_to_user((vga_fb_arg_t *) arg, &vla,
				 sizeof(vga_fb_arg_t)))
			return -EACCES;
		break;

	default:
		return -EINVAL;
	}

	return 0;
}

/* The operations our device knows how to do */
static const struct file_operations vga_fb_fops = {
	.owner		= THIS_MODULE,
	.unlocked_ioctl = vga_fb_ioctl,
};

/* Information about our device for the "misc" framework -- like a char dev */
static struct miscdevice vga_fb_misc_device = {
	.minor		= MISC_DYNAMIC_MINOR,
	.name		= DRIVER_NAME,
	.fops		= &vga_fb_fops,
};

/*
 * Initialization code: get resources (registers) and display
 * a welcome message
 */
static int __init vga_fb_probe(struct platform_device *pdev)
{

	/* Register ourselves as a misc device: creates /dev/vga_fb */
	ret = misc_register(&vga_fb_misc_device);

	/* Get the address of our registers from the device tree */
	ret = of_address_to_resource(pdev->dev.of_node, 0, &dev.res);
	if (ret) {
		ret = -ENOENT;
		goto out_deregister;
	}

	/* Make sure we can use these registers */
	if (request_mem_region(dev.res.start, resource_size(&dev.res),
			       DRIVER_NAME) == NULL) {
		ret = -EBUSY;
		goto out_deregister;
	}

	/* Arrange access to our registers */
	dev.virtbase = of_iomap(pdev->dev.of_node, 0);
	if (dev.virtbase == NULL) {
		ret = -ENOMEM;
		goto out_release_mem_region;
	}

	/* Display a welcome message */
	for (i = 0; i < VGA_PIXELS; i++)
		write_digit(i, 0x0000);

	return 0;

out_release_mem_region:
	release_mem_region(dev.res.start, resource_size(&dev.res));
out_deregister:
	misc_deregister(&vga_fb_misc_device);
	return ret;
}

/* Clean-up code: release resources */
static int vga_fb_remove(struct platform_device *pdev)
{
	iounmap(dev.virtbase);
	release_mem_region(dev.res.start, resource_size(&dev.res));
	misc_deregister(&vga_fb_misc_device);
	return 0;
}

/* Which "compatible" string(s) to search for in the Device Tree */
#ifdef CONFIG_OF
static const struct of_device_id vga_fb_of_match[] = {
	{ .compatible = "altr,vga_fb” },
	{},
};
MODULE_DEVICE_TABLE(of, vga_fb_of_match);
#endif

/* Information for registering ourselves as a "platform" driver */
static struct platform_driver vga_fb_driver = {
	.driver	= {
		.name	= DRIVER_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(vga_fb_of_match),
	},
	.remove	= __exit_p(vga_fb_remove),
};

/* Called when the module is loaded: set things up */
static int __init vga_fb_init(void)
{
	pr_info(DRIVER_NAME ": init\n");
	return platform_driver_probe(&vga_fb_driver, vga_fb_probe);
}

/* Called when the module is unloaded: release resources */
static void __exit vga_fb_exit(void)
{
	platform_driver_unregister(&vga_fb_driver);
	pr_info(DRIVER_NAME ": exit\n");
}

module_init(vga_fb_init);
module_exit(vga_fb_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Stephen A. Edwards, Columbia University");
MODULE_DESCRIPTION("VGA 7-segment LED Emulator");
