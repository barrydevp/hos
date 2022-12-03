BUILD_DIR=.build

BOOTLOADER_BUILD_DIR=$(BUILD_DIR)/bootloader
KERNEL_BUILD_DIR=$(BUILD_DIR)/kernel
DISK_IMG=$(BUILD_DIR)/disk.img

all: qemu

.PHONY: bootloader kernel

qemu: bootdisk
	# qemu-system-i386 -machine q35 -fda $(DISK_IMG) -gdb tcp::26000 -S
	qemu-system-i386 -machine q35 -fda $(DISK_IMG) -gdb tcp::26000
	# qemu-system-i386 -machine q35 -fda $(DISK_IMG) 

disk.dis: bootdisk
	objdump -z -D -b binary -m i686 .build/disk.img | less

# Rule to disassemble the kernel - may be useful to debug
kernel.dis: kernel
	ndisasm -b 32 $(KERNEL_BUILD_DIR)/kernel.bin > $@

bootdisk: bootloader kernel
	dd if=/dev/zero of=$(DISK_IMG) bs=512 count=2880
	dd if=$(BOOTLOADER_BUILD_DIR)/stage1.o of=$(DISK_IMG) bs=512 count=1 seek=0
	dd if=$(BOOTLOADER_BUILD_DIR)/stage2.o of=$(DISK_IMG) bs=512 count=1 seek=1
	dd if=$(KERNEL_BUILD_DIR)/kernel.bin of=$(DISK_IMG) bs=512 count=1 seek=2

bootloader:
	@make -C bootloader

kernel:
	@make -C kernel

clean:
	@rm -r $(BUILD_DIR)/*
