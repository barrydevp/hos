BUILD_DIR=../.build/bootloader
DISK_IMG=$(BUILD_DIR)/disk.img
TEST_DISK_IMG=$(BUILD_DIR)/test_disk.img

BOOTLOADER_SRCS := $(wildcard *.asm)
BOOTLOADER_OBJS := $(patsubst %.asm, $(BUILD_DIR)/%.o, $(BOOTLOADER_SRCS))

all: qemu

qemu: bootdisk
	# qemu-system-i386 -machine q35 -fda $(DISK_IMG) -gdb tcp::26000 -S
	qemu-system-i386 -machine q35 -fda $(DISK_IMG) -gdb tcp::26000
	# qemu-system-i386 -machine q35 -fda $(DISK_IMG) 

test: bootloader
	dd if=/dev/zero of=$(TEST_DISK_IMG) bs=512 count=2880
	dd if=$(BUILD_DIR)/test.o of=$(TEST_DISK_IMG) bs=512 count=3 seek=0
	qemu-system-i386 -machine q35 -fda $(TEST_DISK_IMG) 

bootdisk: bootloader 
	dd if=/dev/zero of=$(DISK_IMG) bs=512 count=2880
	dd if=$(BUILD_DIR)/stage1.o of=$(DISK_IMG) bs=512 count=1 seek=0
	dd if=$(BUILD_DIR)/stage2.o of=$(DISK_IMG) bs=512 count=1 seek=1
	dd if=$(BUILD_DIR)/hello.o of=$(DISK_IMG) bs=512 count=1 seek=2

bootloader: $(BOOTLOADER_OBJS)
# bootloader: $(BUILD_DIR)/stage2.o $(BUILD_DIR)/stage1.o

$(BUILD_DIR)/%.o: %.asm
	@mkdir -p $(BUILD_DIR)
	@nasm -f bin $< -o $@

clean:
	@rm $(BUILD_DIR)/*
