# $@ = target file
# $< = first dependency
# $^ = all dependencies

BUILD_DIR=../.build/kernel

KERNEL_SRCS := $(wildcard *.c)
KERNEL_OBJS := $(patsubst %.c, $(BUILD_DIR)/%.o, $(KERNEL_SRCS))

KERNEL_BIN=$(BUILD_DIR)/kernel.bin
KERNEL_ELF=$(BUILD_DIR)/kernel.elf # kernel binary with elf information for debuging
LINKER_SCRIPT=linker.ld

KERNEL_BOOT_START = 0x100000
TARGET = i686-elf
CC = $(TARGET)-gcc
LD = $(TARGET)-ld
AS = $(TARGET)-as
OBJCOPY = $(TARGET)-objcopy

# -g: Use debuging symbols in gcc
CFLAGS = -g -ffreestanding -O2 -Wall -Wextra
LDFLAGS = -ffreestanding -O2 -nostdlib

.PHONY: multiboot boot

multiboot: $(BUILD_DIR)/kernel_multiboot.bin

boot: $(BUILD_DIR)/kernel_boot.bin

boot/%.bin: $(BUILD_DIR)/%.elf
	$(OBJCOPY) -O binary $^ $@

$(BUILD_DIR)/%.bin: $(BUILD_DIR)/%.elf
	$(OBJCOPY) --strip-debug $^ $@

# Notice how dependencies are built as needed
# $(KERNEL_BIN): $(BOOT_ENTRY) $(KERNEL_OBJS)
# 	# $(CC) $^ -T $(LINKER_SCRIPT) -o $@ $(LDFLAGS) --oformat binary
# 	$(LD) $^ -T $(LINKER_SCRIPT) -o $@ --oformat binary
# 	# $(LD) -o $@ -Ttext $(KERNEL_BOOT_START) $^ --oformat binary

$(BUILD_DIR)/kernel_boot.elf: $(BUILD_DIR)/boot.o $(KERNEL_OBJS)
	$(LD) $^ -T $(LINKER_SCRIPT) -g -o $@

$(BUILD_DIR)/kernel_multiboot.elf: $(BUILD_DIR)/multiboot.o $(KERNEL_OBJS)
	$(CC) $^ -T $(LINKER_SCRIPT) -o $@ $(LDFLAGS) -lgcc

# $(KERNEL_ELF): $(BOOT_ENTRY) $(KERNEL_OBJS)
# 	$(CC) $^ -T $(LINKER_SCRIPT) -o $@ $(LDFLAGS)
# 	$(LD) $^ -T $(LINKER_SCRIPT) -g -o $@
# 	$(LD) -o $@ -Ttext $(KERNEL_BOOT_START) $^

$(BUILD_DIR)/boot.o: boot.asm
	@mkdir -p $(BUILD_DIR)
	@nasm $< -f elf -g -o $@

$(BUILD_DIR)/multiboot.o: multiboot.s
	@mkdir -p $(BUILD_DIR)
	$(AS) $^ -o $@

$(BUILD_DIR)/%.o: %.c
	$(CC) -c $< -o $@ $(CFLAGS) 

clean:
	@rm $(BUILD_DIR)/*
