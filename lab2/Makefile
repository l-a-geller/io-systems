obj-m += main.o

PWD = $(shell pwd)
all:
		make -C /lib/modules/$(shell uname -r)/build M="$(PWD)" modules
clean:
		make -C /lib/modules/$(shell uname -r)/build M="$(PWD)" clean

PARTITIONS = lab2p1 lab2p2 lab2p3

define setup_partition
	mkfs.vfat /dev/$(1)
	mkdir /mnt/$(1)
	mount /dev/$(1) /mnt/$(1);
endef

define umount_partition
	umount /mnt/$(1)
	rmdir /mnt/$(1);
endef


.PHONY: mount
mount:
		$(foreach part, $(PARTITIONS), $(call setup_partition,$(part)))

.PHONY: umount
umount:
		$(foreach part, $(PARTITIONS), $(call umount_partition,$(part)))