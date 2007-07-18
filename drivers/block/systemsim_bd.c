/*
 *  Bogus Block Driver for PowerPC Full System Simulator
 *
 *  (C) Copyright IBM Corporation 2003-2005
 *
 *  Bogus Disk Driver
 *
 *  DISCLAIMER: THIS CODE IS A HACK.  It was done quickly to serve
 *  a particular purpose and has now been repurposed several times.
 *  It is ugly and wrong on several levels, but it works for most
 *  folks.  I will happily integrate any fixups or cleanups that are 
 *  submitted, otherwise stop whinning - its called bogus for a reason.
 *  
 *  Author: Eric Van Hensbegren <bergevan@us.ibm.com>
 *
 *    inspired by drivers/block/nbd.c
 *    written by Pavel Machek and Steven Whitehouse
 *
 *  Some code is from the IBM Full System Simulator Group in ARL
 *  Author: Patrick Bohrer <IBM Austin Research Lab>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to:
 * Free Software Foundation
 * 51 Franklin Street, Fifth Floor
 * Boston, MA  02111-1301  USA
 *
 */

#include <linux/major.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/fs.h>
#include <linux/stat.h>
#include <linux/errno.h>
#include <linux/file.h>
#include <linux/ioctl.h>
#include <linux/blkdev.h>
#include <net/sock.h>
#include <asm/prom.h>
#include <asm/systemsim.h>
#include <asm/prom.h>

#include <linux/uaccess.h>
#include <linux/types.h>

#define MAJOR_NR 42
#define MAX_SYSTEMSIM_BD 128
#define BD_SECT_SZ 512

#define SYSTEMSIM_BD_SET_BLKSIZE _IO( 0xab, 1 )
#define SYSTEMSIM_BD_SET_SIZE    _IO( 0xab, 2 )
#define SYSTEMSIM_BD_SET_SIZE_BLOCKS     _IO( 0xab, 7 )
#define SYSTEMSIM_BD_DISCONNECT  _IO( 0xab, 8 )

struct systemsim_bd_device {
	int initialized;
	int refcnt;
	int flags;
	struct gendisk *disk;
};

static struct systemsim_bd_device systemsim_bd_dev[MAX_SYSTEMSIM_BD];

#define BD_INFO_SYNC   0
#define BD_INFO_STATUS 1
#define BD_INFO_BLKSZ  2
#define BD_INFO_DEVSZ  3
#define BD_INFO_CHANGE 4

#define BOGUS_DISK_READ  116
#define BOGUS_DISK_WRITE 117
#define BOGUS_DISK_INFO  118

static inline int
systemsim_disk_read(int devno, void *buf, ulong sect, ulong nrsect)
{
	memset(buf, 0, nrsect*BD_SECT_SZ);
	return callthru3(BOGUS_DISK_READ, (unsigned long)buf,
			 (unsigned long)sect,
			 (unsigned long)((nrsect << 16) | devno));
}

static inline int
systemsim_disk_write(int devno, void *buf, ulong sect, ulong nrsect)
{
	return callthru3(BOGUS_DISK_WRITE, (unsigned long)buf,
			 (unsigned long)sect,
			 (unsigned long)((nrsect << 16) | devno));
}

static inline int systemsim_disk_info(int op, int devno)
{
	return callthru2(BOGUS_DISK_INFO, (unsigned long)op,
			 (unsigned long)devno);
}

static int systemsim_bd_init_disk(int devno)
{
	struct gendisk *disk = systemsim_bd_dev[devno].disk;
	unsigned int sz;

	/* check disk configured */
	if (!systemsim_disk_info(BD_INFO_STATUS, devno)) {
		printk(KERN_ERR
		       "Attempting to open bogus disk before initializaiton\n");
		return 0;
	}

	systemsim_bd_dev[devno].initialized++;

	sz = systemsim_disk_info(BD_INFO_DEVSZ, devno);

	printk("Initializing disk %d with devsz %u\n", devno, sz);

	set_capacity(disk, sz << 1);

	return 1;
}

static void do_systemsim_bd_request(request_queue_t *q)
{
	int result = 0;
	struct request *req;

	while ((req = elv_next_request(q)) != NULL) {
		int minor = req->rq_disk->first_minor;

		switch (rq_data_dir(req)) {
		case READ:
			result = systemsim_disk_read(minor,
						     req->buffer, req->sector,
						     req->current_nr_sectors);
			break;
		case WRITE:
			result = systemsim_disk_write(minor,
						      req->buffer, req->sector,
						      req->current_nr_sectors);
		};

		if (result)
			end_request(req, 0);	/* failure */
		else
			end_request(req, 1);	/* success */
	}
}

static int systemsim_bd_release(struct inode *inode, struct file *file)
{
	struct systemsim_bd_device *lo;
	int dev;

	if (!inode)
		return -ENODEV;
	dev = inode->i_bdev->bd_disk->first_minor;
	if (dev >= MAX_SYSTEMSIM_BD)
		return -ENODEV;
	if (systemsim_disk_info(BD_INFO_SYNC, dev) < 0) {
		printk(KERN_ALERT "systemsim_bd_release: unable to sync\n");
	}
	lo = &systemsim_bd_dev[dev];
	if (lo->refcnt <= 0)
		printk(KERN_ALERT "systemsim_bd_release: refcount(%d) <= 0\n",
		       lo->refcnt);
	lo->refcnt--;
	return 0;
}

static int systemsim_bd_revalidate(struct gendisk *disk)
{
	int devno = disk->first_minor;

	systemsim_bd_init_disk(devno);

	return 0;
}

static int systemsim_bd_open(struct inode *inode, struct file *file)
{
	int dev;

	if (!inode)
		return -EINVAL;
	dev = inode->i_bdev->bd_disk->first_minor;
	if (dev >= MAX_SYSTEMSIM_BD)
		return -ENODEV;

	check_disk_change(inode->i_bdev);

	if (!systemsim_bd_dev[dev].initialized)
		if (!systemsim_bd_init_disk(dev))
			return -ENODEV;

	systemsim_bd_dev[dev].refcnt++;
	return 0;
}

static struct block_device_operations systemsim_bd_fops = {
	.owner =		THIS_MODULE,
	.open =			systemsim_bd_open,
	.release =		systemsim_bd_release,
	.revalidate_disk =	systemsim_bd_revalidate,
};

static spinlock_t systemsim_bd_lock = SPIN_LOCK_UNLOCKED;

static int __init systemsim_bd_init(void)
{
	struct device_node *systemsim;
	int err = -ENOMEM;
	int i;

	systemsim = of_find_node_by_path("/systemsim");

	if (systemsim == NULL) {
		printk(KERN_ERR "NO SYSTEMSIM BOGUS DISK DETECTED\n");
		return -ENODEV;
	}

	/*
	 * We could detect which disks are configured in openfirmware
	 * but I think this unnecessarily limits us from being able to
	 * hot-plug bogus disks durning run-time.
	 *
	 */

	for (i = 0; i < MAX_SYSTEMSIM_BD; i++) {
		struct gendisk *disk = alloc_disk(1);
		if (!disk)
			goto out;
		systemsim_bd_dev[i].disk = disk;
		/*
		 * The new linux 2.5 block layer implementation requires
		 * every gendisk to have its very own request_queue struct.
		 * These structs are big so we dynamically allocate them.
		 */
		disk->queue =
		    blk_init_queue(do_systemsim_bd_request, &systemsim_bd_lock);
		if (!disk->queue) {
			put_disk(disk);
			goto out;
		}
	}

	if (register_blkdev(MAJOR_NR, "systemsim_bd")) {
		err = -EIO;
		goto out;
	}
#ifdef MODULE
	printk(KERN_INFO "systemsim bogus disk: device at major %d\n",
	       MAJOR_NR);
#else
	printk(KERN_INFO "systemsim bogus disk: compiled in with kernel\n");
#endif

	/*
	 * left device name alone for now as too much depends on it
	 * external to the kernel
	 *
	 */

	for (i = 0; i < MAX_SYSTEMSIM_BD; i++) {	/* load defaults */
		struct gendisk *disk = systemsim_bd_dev[i].disk;
		systemsim_bd_dev[i].initialized = 0;
		systemsim_bd_dev[i].refcnt = 0;
		systemsim_bd_dev[i].flags = 0;
		disk->major = MAJOR_NR;
		disk->first_minor = i;
		disk->fops = &systemsim_bd_fops;
		disk->private_data = &systemsim_bd_dev[i];
		sprintf(disk->disk_name, "mambobd%d", i);
		set_capacity(disk, 0x7ffffc00ULL << 1);	/* 2 TB */
		add_disk(disk);
	}

	return 0;
out:
	while (i--) {
		if (systemsim_bd_dev[i].disk->queue)
			blk_cleanup_queue(systemsim_bd_dev[i].disk->queue);
		put_disk(systemsim_bd_dev[i].disk);
	}
	return -EIO;
}

static void __exit systemsim_bd_cleanup(void)
{

	if (unregister_blkdev(MAJOR_NR, "systemsim_bd") != 0)
		printk(KERN_ERR "systemsim_bd: cleanup_module failed\n");
	else
		printk(KERN_INFO "systemsim_bd: module cleaned up.\n");
}

module_init(systemsim_bd_init);
module_exit(systemsim_bd_cleanup);

MODULE_DESCRIPTION("Systemsim Block Device");
MODULE_LICENSE("GPL");
