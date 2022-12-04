# $@ = target file
# $< = first dependency
# $^ = all dependencies
#
TARGET:=i686-elf
CC:=$(TARGET)-gcc
LD:=$(TARGET)-ld
AS:=$(TARGET)-as
OBJCOPY = $(TARGET)-objcopy

# -g: Use debuging symbols in gcc
CFLAGS = -g -ffreestanding -Wall -Wextra
LDFLAGS = -ffreestanding -nostdlib

# optimized build flags, non debugging
# CFLAGS = -ffreestanding -O2 -Wall -Wextra
# LDFLAGS = -ffreestanding -O2 -nostdlib

BUILD_DIR=../.build/kernel

KERNEL_SRCS := $(wildcard *.c)
KERNEL_OBJS := $(patsubst %.c, $(BUILD_DIR)/%.o, $(KERNEL_SRCS))

LINKER_SCRIPT=linker.ld


.PHONY: multiboot boot

multiboot: $(BUILD_DIR)/kernel_multiboot.bin

boot: $(BUILD_DIR)/kernel_boot.bin

boot/%.bin: $(BUILD_DIR)/%.elf
	$(OBJCOPY) -O binary $^ $@

$(BUILD_DIR)/%.bin: $(BUILD_DIR)/%.elf
	$(OBJCOPY) --strip-debug $^ $@

$(BUILD_DIR)/kernel_boot.elf: $(BUILD_DIR)/boot.o $(KERNEL_OBJS)
	$(LD) $^ -T $(LINKER_SCRIPT) -o $@

$(BUILD_DIR)/kernel_multiboot.elf: $(BUILD_DIR)/multiboot.o $(KERNEL_OBJS)
	$(CC) $^ -T $(LINKER_SCRIPT) -o $@ $(LDFLAGS) -lgcc

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
