BUILD_DIR=../.build/bootloader
DISK_IMG=$(BUILD_DIR)/disk.img
STAGE1=$(BUILD_DIR)/stage1.o
STAGE2=$(BUILD_DIR)/stage2.o
OS=$(BUILD_DIR)/second.o

BOOTLOADER_SRCS := $(wildcard *.asm)
BOOTLOADER_OBJS := $(patsubst %.asm, $(BUILD_DIR)/%.o, $(BOOTLOADER_SRCS))

all: qemu

qemu: bootdisk
	# qemu-system-i386 -machine q35 -fda $(DISK_IMG) -gdb tcp::26000 -S
	# qemu-system-i386 -machine q35 -fda $(DISK_IMG) -gdb tcp::26000
	qemu-system-i386 -machine q35 -fda $(DISK_IMG) 

bootdisk: bootloader 
	dd if=/dev/zero of=$(DISK_IMG) bs=512 count=2880
	dd if=$(STAGE1) of=$(DISK_IMG) bs=512 count=1 seek=0
	dd if=$(STAGE2) of=$(DISK_IMG) bs=512 count=$$(($(shell stat --printf="%s" $(STAGE2))/512+1)) seek=1

# bootloader: $(BOOTLOADER_OBJS)
bootloader: $(BUILD_DIR)/stage2.o $(BUILD_DIR)/stage1.o

$(BUILD_DIR)/%.o: %.asm
	@mkdir -p $(BUILD_DIR)
	@nasm -f bin $< -o $@

clean:
	@rm $(BUILD_DIR)/*
