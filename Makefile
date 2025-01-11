include make.config
include $(SRC)/arch/$(ARCH)/make.config

# CFLAGS for kernel objects and modules
K_CFLAGS  = -ffreestanding -O2 -g -lgcc
K_CFLAGS += -Wall -Wextra -Wno-unused-function -Wno-unused-parameter -Wstrict-prototypes -Wwrite-strings -Wno-int-conversion
K_CFLAGS += ${ARCH_KERNEL_CFLAGS}

# Defined constants for the kernel
K_CFLAGS += -D_KERNEL_ -DKERNEL_ARCH=${ARCH}
K_CFLAGS += -DKERNEL_GIT_TAG=0.0.0

# LDFLAGS for kernel
# K_LDFLAGS  = -ffreestanding -O2 -nostdlib -lgcc
K_LDFLAGS  = -ffreestanding -nostdlib -lgcc # debug purpose => turn off O2 optimization

# These sources are used to determine if we should update symbols.o
KERNEL_SRCS  = $(wildcard $(SRC)/kernel/*.c)
KERNEL_SRCS += $(wildcard $(SRC)/kernel/*/*.c)
KERNEL_SRCS += $(wildcard $(SRC)/kernel/*/*/*.c)

# Automatically find kernel sources from relevant paths
KERNEL_OBJS  = $(patsubst %.c,%.o,$(wildcard $(SRC)/kernel/*.c))
KERNEL_OBJS += $(patsubst %.c,%.o,$(wildcard $(SRC)/kernel/*/*.c))
KERNEL_OBJS += $(patsubst %.c,%.o,$(wildcard $(SRC)/kernel/*/*/*.c))

# Assembly sources only come from the arch-dependent directory
KERNEL_ASMOBJS  = $(filter-out $(SRC)/kernel/symbols.o,$(patsubst %.S,%.o,$(wildcard $(SRC)/kernel/arch/${ARCH}/*.S)))

# Kernel Linker script
KERNEL_LINKERLD = $(SRC)/arch/$(ARCH)/linker.ld

# kernel Header

KERNEL_HDRS  = $(wildcard include/kernel/*.h)
KERNEL_HDRS += $(wildcard include/kernel/*/*.h)
KERNEL_HDRS += $(wildcard include/kernel/*/*/*.h)

# target for rust build
RUST_TARGET = ${ARCH}-unknown-hos
RUST_LIB_DIR = target/${RUST_TARGET}/debug
# Rust sources library
RS_KERNEL_SRCS = $(wildcard $(SRC)/rskernel/**/*.rs)
RS_KERNEL_LIB = ${RUST_LIB_DIR}/librskernel.a

# The arch sources file
ARCH_SRCS  = $(wildcard $(SRC)/arch/${ARCH}/*.c)
ARCH_SRCS += $(wildcard $(SRC)/arch/${ARCH}/*/*.c)
ARCH_SRCS += $(wildcard $(SRC)/arch/${ARCH}/*.S)
ARCH_SRCS += $(wildcard $(SRC)/arch/${ARCH}/*.asm)

# Automatically find arch sources from relevant paths
ARCH_OBJS  = $(patsubst %.c,%.o,$(wildcard $(SRC)/arch/${ARCH}/*.c))
ARCH_OBJS += $(patsubst %.c,%.o,$(wildcard $(SRC)/arch/${ARCH}/*/*.c))

# Assembly sources only come from the arch-dependent directory
ARCH_OBJS += $(patsubst %.S,%.o,$(wildcard $(SRC)/arch/${ARCH}/*.S))
ARCH_OBJS += $(patsubst %.S,%.o,$(wildcard $(SRC)/arch/${ARCH}/*/*.S))
ARCH_OBJS += $(patsubst %.asm,%.o,$(wildcard $(SRC)/arch/${ARCH}/*.asm))

# Arch Header
ARCH_HDRS  = $(wildcard include/arch/${ARCH}/*.h)
ARCH_HDRS += $(wildcard include/arch/${ARCH}/*/*.h)

# Kernel modules are one file = one module; if you want to build more complicated
# modules, you could potentially use `ld -r` to turn multiple source objects into
# a single relocatable object file.
# ARCH_ENABLED_MODS = $(shell util/valid-modules.sh $(ARCH))
# MODULES = $(patsubst modules/%.c,$(BASE)/mod/%.ko,$(foreach mod,$(ARCH_ENABLED_MODS),modules/$(mod).c))

EMU = qemu-system-${ARCH}
EMU_FLAGS ?= -m 256M
EMU_FLAGS += -drive file=rootfs.img,format=raw,index=0,media=disk

ifeq ($(DEBUG), 1)
	EMU_FLAGS += -s -S
endif

APPS=$(patsubst apps/%.c,%,$(wildcard apps/*.c))
APPS_X=$(foreach app,$(APPS),$(BASE)/bin/$(app))
APPS_X_O=$(foreach app,$(APPS),$(BASE)/bin/$(app).o)
APPS_Y=$(foreach app,$(APPS),.make/$(app).mak)
APPS_SH=$(patsubst apps/%.sh,%.sh,$(wildcard apps/*.sh))
APPS_SH_X=$(foreach app,$(APPS_SH),$(BASE)/bin/$(app))

LIBS=$(patsubst $(SRC)/lib/%.c,%,$(wildcard lib/*.c))
LIBS_X=$(foreach lib,$(LIBS),$(BASE)/lib/libtoaru_$(lib).so)
LIBS_Y=$(foreach lib,$(LIBS),.make/$(lib).lmak)

# CFLAGS for user mode
# CFLAGS  = -O2 -g
CFLAGS  = -O3
# CFLAGS += -Wall -Wextra -Wno-unused-parameter
# CFLAGS += -Iapps
# CFLAGS += -fplan9-extensions ${ARCH_USER_CFLAGS}
# CFLAGS += -ffreestanding -nostdlib
CFLAGS += -u_start

# Warning flags.
CFLAGS += -Wall
CFLAGS += -Werror
CFLAGS += -Wpedantic
CFLAGS += -pedantic-errors
CFLAGS += -Wshadow
CFLAGS += -std=gnu99

# Disable some specific warnings.
CFLAGS += -Wno-unused-function
CFLAGS += -Wno-unused-variable
CFLAGS += -Wno-unknown-pragmas
CFLAGS += -Wno-missing-braces
CFLAGS += -Wno-unused-command-line-argument

# Set the compiler options.
CFLAGS += -static
CFLAGS += -nostdlib
CFLAGS += -nostdinc
CFLAGS += -fno-builtin
CFLAGS += -fno-stack-protector
CFLAGS += -fno-pic
CFLAGS += -fomit-frame-pointer
CFLAGS += -m32
CFLAGS += -march=i686

LIBC_OBJS  = $(patsubst %.c,%.o,$(wildcard $(SRC)/libc/*.c))
LIBC_OBJS += $(patsubst %.c,%.o,$(wildcard $(SRC)/libc/*/*.c))
LIBC_OBJS += $(patsubst %.c,%.o,$(wildcard $(SRC)/libc/arch/${ARCH}/*.c))

GCC_SHARED = $(BASE)/usr/lib/libgcc_s.so.1 $(BASE)/usr/lib/libgcc_s.so

CRTBEGIN_OBJ:=$(shell $(CC) $(CFLAGS) -print-file-name=crtbegin.o)
CRTEND_OBJ:=$(shell $(CC) $(CFLAGS) -print-file-name=crtend.o)
CRTS = $(SRC)/libc/arch/$(ARCH)/crti.o $(CRTBEGIN_OBJ) $(CRTEND_OBJ) $(SRC)/libc/arch/$(ARCH)/crtn.o
# CRTS = $(BASE)/lib/crt0.o $(BASE)/lib/crti.o $(BASE)/lib/crtn.o

LIBC = $(BASE)/lib/libc.so $(GCC_SHARED)

PHONY += all all-c system clean run shell

all: qemu-kernel

all-c: qemu-c-kernel

all-cgui: qemu-c-kernel-gui

all-cvnc: qemu-c-kernel-vnc

PHONY += qemu-kernel qemu-c-kernel qemu-c-kernel-gui qemu-c-kernel-vnc

qemu-kernel: hos.kernel
	$(EMU) $(EMU_FLAGS) -nographic -display curses -kernel $^
	# $(EMU) $(EMU_FLAGS) -kernel $^ -s -S

qemu-c-kernel: hos.iso rootfs.img
	# $(EMU) $(EMU_FLAGS) -nographic -display curses -serial file:c.kernel.log -kernel hos.kernel
	$(EMU) $(EMU_FLAGS) -nographic -display curses -serial file:c.kernel.log -boot d -cdrom hos.iso
	# $(EMU) $(EMU_FLAGS) -nographic -display curses -s -S -serial file:c.kernel.log -kernel hos.kernel
	# $(EMU) $(EMU_FLAGS) -s -S -serial file:c.kernel.log -kernel hos.kernel

qemu-c-kernel-gui: hos.iso rootfs.img
	$(EMU) $(EMU_FLAGS) -serial stdio -boot d -cdrom hos.iso

qemu-c-kernel-vnc: hos.iso rootfs.img
	printf "change vnc password\n%s\n" 1 | $(EMU) $(EMU_FLAGS) \
		-vnc :0,password=on \
		-serial file:c.kernel.log \
		-monitor stdio \
		-boot d \
		-cdrom hos.iso

PHONY += hos.bin

rootfs.img:
	-bash create_filesystem.sh

hos.iso: hos.bin
	-cp multiboot/grub.cfg isodir/boot/grub/grub.cfg
	-grub-mkrescue -o hos.iso isodir

hos.bin: hos.c.kernel
	-mkdir -p isodir/boot/grub
	-cp hos.c.kernel isodir/boot/hos.bin

hos.c.kernel: $(KERNEL_LINKERLD) $(KERNEL_OBJS) $(ARCH_OBJS) $(APPS_X_O)
	$(CC) -T $(KERNEL_LINKERLD) $(K_LDFLAGS) -o $@ $(ARCH_OBJS) $(KERNEL_OBJS) $(APPS_X_O)

hos.kernel: $(KERNEL_LINKERLD) $(ARCH_OBJS) $(CRTS)
	$(CC) -T $(KERNEL_LINKERLD) $(K_CFLAGS) -nostdlib -o $@ $(ARCH_OBJS) $(CRTS) $(RS_KERNEL_LIB)

$(SRC)/kernel/sys/version.o: ${KERNEL_SRCS}

$(SRC)/kernel/%.o: $(SRC)/kernel/%.S
	${AS} $< -o $@ 
	# ${CC} $(K_CFLAGS) -c $< -o $@ 

$(SRC)/arch/%.o: $(SRC)/arch/%.S
	${AS} $< -o $@ 
	# ${CC} $(K_CFLAGS) -c $< -o $@ 

$(SRC)/kernel/%.o: $(SRC)/kernel/%.c ${KERNEL_HDRS} ${ARCH_HDRS}
	${CC} ${K_CFLAGS} -nostdlib -Iinclude -c -o $@ $<

$(SRC)/arch/%.o: $(SRC)/arch/%.c ${KERNEL_HDRS} ${ARCH_HDRS}
	${CC} ${K_CFLAGS} -nostdlib -Iinclude -c -o $@ $<

$(RUST_LIB_DIR)/%.a: ${RS_KERNEL_SRCS}
	${CARGO} build

clean:
	-rm -f ${KERNEL_ASMOBJS}
	-rm -f ${KERNEL_OBJS} $(MODULES)
	-rm -f ${ARCH_OBJS}
	-rm -f kernel/symbols.o kernel/symbols.S misaka-kernel misaka-kernel.64
	-rm -f ramdisk.tar ramdisk.igz 
	-rm -f $(APPS_Y) $(LIBS_Y) $(KRK_MODS_Y) $(KRK_MODS)
	-rm -f $(APPS_X) $(LIBS_X) $(KRK_MODS_X) $(APPS_KRK_X) $(APPS_SH_X)
	-rm -f $(BASE)/lib/crt0.o $(BASE)/lib/crti.o $(BASE)/lib/crtn.o
	-rm -f $(BASE)/lib/libc.so $(BASE)/lib/libc.a
	-rm -f $(LIBC_OBJS) $(BASE)/lib/ld.so $(BASE)/lib/libkuroko.so $(BASE)/lib/libm.so
	-rm -f $(BASE)/bin/kuroko
	-rm -f $(GCC_SHARED)
	-rm -f boot/efi/*.o boot/bios/*.o
	-$(CARGO) clean
	-rm -f hos.c.kernel
	-rm -f hos.kernel
	-rm -rf isodir
	-rm -f hos.iso
	# -rm -f rootfs.img


$(BASE)/lib/ld.so: linker/linker.c $(BASE)/lib/libc.a | dirs $(LIBC)
	$(CC) -g -static -Wl,-static $(CFLAGS) -z max-page-size=0x1000 -o $@ -Os -T linker/link.ld $<

$(SRC)/libc/%.o: $(SRC)/libc/%.c base/usr/include/syscall.h 
	$(CC) -O2 -std=gnu11 -ffreestanding -Wall -Wextra -Wno-unused-parameter -fPIC -c -o $@ $<

PHONY += libc

$(SRC)/libc: $(BASE)/lib/libc.a $(BASE)/lib/libc.so

$(BASE)/lib/libc.a: ${LIBC_OBJS} $(CRTS)
	$(AR) cr $@ $(LIBC_OBJS)

$(BASE)/lib/libc.so: ${LIBC_OBJS} | $(CRTS)
	${CC} -nodefaultlibs -shared -fPIC -o $@ $^ -lgcc

$(BASE)/lib/crt%.o: libc/arch/${ARCH}/crt%.o

$(SRC)/libc/arch/${ARCH}/crt%.o: $(SRC)/libc/arch/${ARCH}/crt%.S
	${CC} -c $< -o $@ 

$(BASE)/usr/lib/%: $(TOOLCHAIN)/local/${TARGET}/lib/% | dirs
	cp -a $< $@
	-$(STRIP) $@

$(BASE)/lib/libm.so: util/libm.c
	$(CC) -shared -nostdlib -fPIC -o $@ $<

$(BASE)/dev:
	mkdir -p $@
$(BASE)/tmp:
	mkdir -p $@
$(BASE)/proc:
	mkdir -p $@
$(BASE)/bin:
	mkdir -p $@
$(BASE)/lib:
	mkdir -p $@
$(BASE)/cdrom:
	mkdir -p $@
$(BASE)/var:
	mkdir -p $@
$(BASE)/mod:
	mkdir -p $@
$(BASE)/usr/lib:
	mkdir -p $@
$(BASE)/usr/bin:
	mkdir -p $@
boot/efi:
	mkdir -p $@
boot/bios:
	mkdir -p $@
fatbase/efi/boot:
	mkdir -p $@
cdrom:
	mkdir -p $@
.make:
	mkdir -p .make
dirs: $(BASE)/dev $(BASE)/tmp $(BASE)/proc $(BASE)/bin $(BASE)/lib $(BASE)/cdrom $(BASE)/usr/lib $(BASE)/usr/bin cdrom $(BASE)/var fatbase/efi/boot .make $(BASE)/mod boot/efi boot/bios

ifeq (,$(findstring clean,$(MAKECMDGOALS)))
-include ${APPS_Y}
-include ${LIBS_Y}
endif

$(BASE)/bin/%.sh: apps/%.sh
	cp $< $@
	chmod +x $@

PHONY += libs
libs: $(LIBS_X)

.PRECIOUS: apps/%.o $(APPS_X)

PHONY += apps clean-app
apps: $(APPS_X_O)

clean-app:
	-rm -f ${APPS_X} ${APPS_X_O}

APP_HDRS = $(wildcard apps/*.h)
APP_LINKERLD = $(APP_SRC)/linker.ld

$(BASE)/bin/%.o: $(BASE)/bin/%
	objcopy -I binary -O elf32-i386 -B i386 $< $@ 

$(BASE)/bin/%: $(APP_SRC)/%.o $(APP_HDRS) $(APP_LINKERLD) $(APP_SRC)/crt0.o 
	# ${CC} -T $(APP_LINKERLD) ${CFLAGS} -Iapps -o $@ $<
	${CC} ${CFLAGS} -Iapps -o $@ $(APP_SRC)/crt0.o $<

$(APP_SRC)/%.o: $(APP_SRC)/%.c $(APP_HDRS) $(APP_LINKERLD)
	# ${CC} -T $(APP_LINKERLD) ${CFLAGS} -Iapps -c -o $@ $<
	${CC} ${CFLAGS} -Iapps -c -o $@ $<

$(APP_SRC)/crt%.o: $(APP_SRC)/crt%.S $(APP_HDRS) $(APP_LINKERLD)
	${CC} ${CFLAGS} -Iapps -c -o $@ $<

TESTFLAGS := -std=gnu99 \
        -I tests/include \
        -I include \
        -DSMALL_PAGES=$(SMALL_PAGES) \
        -D_TEST_=1

PHONY += test
test: 
	gcc ${TESTFLAGS} $(TESTS)/kernel/memory.c $(SRC)/kernel/memory/pmm.c $(SRC)/kernel/multiboot.c -o tests/kernel/memory.test
	./$(TESTS)/kernel/memory.test

SOURCE_FILES  = $(wildcard $(SRC)/kernel/*.c $(SRC)/kernel/*/*.c $(SRC)/kernel/*/*/*.c $(SRC)/kernel/*/*/*/*.c)
SOURCE_FILES += $(wildcard $(SRC)/apps/*.c $(SRC)/linker/*.c $(SRC)/libc/*.c $(SRC)/libc/*/*.c $(SRC)/lib/*.c)
SOURCE_FILES += $(wildcard $(BASE)/usr/include/*.h $(BASE)/usr/include/*/*.h $(BASE)/usr/include/*/*/*.h)

tags: $(SOURCE_FILES)
	ctags -f tags $(SOURCE_FILES)

.PHONY: $(PHONY)
