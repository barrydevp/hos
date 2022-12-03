# $@ = target file
# $< = first dependency
# $^ = all dependencies

BUILD_DIR=../.build/kernel

KERNEL_SRCS := $(wildcard *.c)
KERNEL_OBJS := $(patsubst %.c, $(BUILD_DIR)/%.o, $(KERNEL_SRCS))
KERNEL_ENTRY := $(BUILD_DIR)/kernel_entry.o
KERNEL_BIN=$(BUILD_DIR)/kernel.bin

KERNEL_BOOT_START := 0x100000
TARGET := i686-elf
CC := $(TARGET)-gcc
LD := $(TARGET)-ld

all: kernel.bin

kernel.bin: $(KERNEL_BIN)

# Notice how dependencies are built as needed
$(KERNEL_BIN): $(KERNEL_ENTRY) $(KERNEL_OBJS)
	$(LD) -o $(KERNEL_BIN) -Ttext $(KERNEL_BOOT_START) $^ --oformat binary

$(KERNEL_ENTRY): kernel_entry.asm
	@mkdir -p $(BUILD_DIR)
	nasm $< -f elf -o $@

$(BUILD_DIR)/%.o: %.c
	$(CC) -ffreestanding -c $< -o $@

clean:
	@rm $(BUILD_DIR)/*
