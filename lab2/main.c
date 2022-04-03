#include "driver.h"
#define QUEUE_DEPTH 128

static int major;
static struct blk_dev disk;

static struct partition_entry partitions[] = 
{
	{
		bootable: 0x00,
		start_head: 0x00,
		start_cyl_sec: 0x0000,
		part_type: 0x83,
		end_head: 0x00,
		end_cyl_sec: 0x0000,
		abs_start_sec: 0x1,
		nr_sec: 0x4fff // 10 MB
	},
	{
		bootable: 0x00,
		start_head: 0x00,
		start_cyl_sec: 0x0000,
		part_type: 0x83,
		end_head: 0x00,
		end_cyl_sec: 0x0000,
		abs_start_sec: 0x5000,
		nr_sec: 0xC800 // 25 MB
	},
	{
		bootable: 0x00,
		start_head: 0x00,
		start_cyl_sec: 0x0000,
		part_type: 0x83,
		end_head: 0x00,
		end_cyl_sec: 0x0000,
		abs_start_sec: 0x11800,
		nr_sec: 0x7800 // 15 MB
	},
};

static void write_mbr(u8 *buf) 
{
	u16 br_signature;

	memset(buf, 0, BR_SIZE);
	memcpy(buf + BR_PARTITION_TABLE_OFFSET, &partitions, sizeof(partitions));

	br_signature = BR_SIGNATURE;
	memcpy(buf + BR_SIGNATURE_OFFSET, &br_signature, sizeof(br_signature));
}

static int do_transfer(struct request *rq, unsigned int *nr_bytes)
{
	int ret = 0;
	struct bio_vec bvec;
	struct req_iterator iter;
	loff_t pos = blk_rq_pos(rq) << SECTOR_SHIFT;

	rq_for_each_segment(bvec, rq, iter) {
		unsigned long b_len = bvec.bv_len;
		void *b_buf = page_address(bvec.bv_page) + bvec.bv_offset;

		if (rq_data_dir(rq) == WRITE)
			memcpy(disk.data + pos, b_buf, b_len);
		else
			memcpy(b_buf, disk.data + pos, b_len);

		pos += b_len;
		*nr_bytes += b_len;
	}
	return ret;
}

static blk_status_t queue_rq(struct blk_mq_hw_ctx *hctx,
				const struct blk_mq_queue_data *bd)
{
	unsigned int nr_bytes = 0;
	blk_status_t status = BLK_STS_OK;

	blk_mq_start_request(bd->rq);

	if (do_transfer(bd->rq, &nr_bytes) != 0 ||
			blk_update_request(bd->rq, status, nr_bytes))
		status = BLK_STS_IOERR;

	blk_mq_end_request(bd->rq, status);
	return status;
}

static const struct blk_mq_ops mq_ops = {
	.queue_rq = queue_rq,
};


static int disk_open(struct block_device *bdev, fmode_t mode) 
{
    pr_info("%s: disk open\n", THIS_MODULE->name);
    return 0;
}

void disk_release(struct gendisk *gd, fmode_t mode) 
{
    pr_info("%s: disk release\n", THIS_MODULE->name);
}

struct block_device_operations disk_ops = 
{
    .owner = THIS_MODULE,
    .open = disk_open,
    .release = disk_release
};

static int __init mod_init(void) 
{
	u8 minors;
	disk.data = vmalloc(DISK_SIZE);
	if (!disk.data) {
		pr_err("%s: failed to allocate memory for disk: size=%d\n", THIS_MODULE->name, DISK_SIZE);
		return -1;
	}
	write_mbr(disk.data);

	if ((major = register_blkdev(0, DISK_NAME)) < 0) {
		pr_err("%s: failed to get major number\n", THIS_MODULE->name);
		vfree(disk.data);
		return -1;
	}

	disk.queue = blk_mq_init_sq_queue(&disk.tag_set,
					  &mq_ops,
					  QUEUE_DEPTH,
					  BLK_MQ_F_SHOULD_MERGE);
	if (!disk.queue) {
		pr_err("%s: failed to init blk queue\n", THIS_MODULE->name);
		unregister_blkdev(major, DISK_NAME);
		vfree(disk.data);
		return -1;
	}

	minors = 4;
	disk.gd = alloc_disk(minors);
	if (!disk.gd) {
		pr_err("%s: failed to allocate disk\n", THIS_MODULE->name);
		blk_cleanup_queue(disk.queue);
		unregister_blkdev(major, DISK_NAME);
		vfree(disk.data);
		return -1;
	}

	disk.gd->major = major;
	disk.gd->first_minor = 0;
	disk.gd->fops = &disk_ops;
	disk.gd->queue = disk.queue;
	strcpy(disk.gd->disk_name, DISK_NAME);
	set_capacity(disk.gd, DISK_NR_SECTORS);
	add_disk(disk.gd);

	pr_info("%s: successfully loaded: disk=%s, major=%d\n",
	        THIS_MODULE->name, DISK_NAME, major);

	return 0;
}

static void __exit mod_exit(void) {
	del_gendisk(disk.gd);
	blk_cleanup_queue(disk.queue);
	unregister_blkdev(major, DISK_NAME);
	vfree(disk.data);
	printk(KERN_INFO "%s: successfully release all resources\n", THIS_MODULE->name);
}

module_init(mod_init);
module_exit(mod_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Margarita Blinova & Geller Leonid P33301");
MODULE_DESCRIPTION("IO-systems lab2 - block device driver");