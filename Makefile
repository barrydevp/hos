TOOLCHAIN=tools
BASE=base
# export PATH := $(shell $(TOOLCHAIN)/activate.sh)

# DEFAULT_HOST != (shell $(TOOLCHAIN)/default-host.sh)
# HOST ?= DEFAULT_HOST
# ARCH != $(shell $(TOOLCHAIN)/target-triplet-to-arch.sh $(HOST))

DEFAULT_HOST=i686-elf
HOST=DEFAULT_HOST
ARCH=i386

PHONY =

# Cross compiler binaries
CC = ${TARGET}-gcc
NM = ${TARGET}-nm
CXX= ${TARGET}-g++
AR = ${TARGET}-ar
AS = ${TARGET}-as
OC = ${TARGET}-objcopy
STRIP= ${TARGET}-strip

include make.config
include arch/$(ARCH)/make.config

# CFLAGS for kernel objects and modules
K_CFLAGS  = -ffreestanding -O2 -g
K_CFLAGS += -Wall -Wextra -Wno-unused-function -Wno-unused-parameter -Wstrict-prototypes -Wwrite-strings
K_CFLAGS += ${ARCH_KERNEL_CFLAGS}

# Defined constants for the kernel
K_CFLAGS += -D_KERNEL_ -DKERNEL_ARCH=${ARCH}
K_CFLAGS += -DKERNEL_GIT_TAG=$(shell util/make-version)

# These sources are used to determine if we should update symbols.o
KERNEL_SRCS  = $(wildcard kernel/*.c)
KERNEL_SRCS += $(wildcard kernel/*/*.c)

# Automatically find kernel sources from relevant paths
KERNEL_OBJS  = $(patsubst %.c,%.o,$(wildcard kernel/*.c))
KERNEL_OBJS += $(patsubst %.c,%.o,$(wildcard kernel/*/*.c))

# Assembly sources only come from the arch-dependent directory
KERNEL_ASMOBJS  = $(filter-out kernel/symbols.o,$(patsubst %.S,%.o,$(wildcard kernel/arch/${ARCH}/*.S)))

# The arch sources file
ARCH_SRCS  = $(wildcard arch/${ARCH}/*.c)
ARCH_SRCS += $(wildcard arch/${ARCH}/*/*.c)
ARCH_SRCS += $(wildcard kernel/arch/${ARCH}/*.S)
ARCH_SRCS += $(wildcard kernel/arch/${ARCH}/*.asm)

# Automatically find arch sources from relevant paths
ARCH_OBJS  = $(patsubst %.c,%.o,$(wildcard arch/${ARCH}/*.c))
ARCH_OBJS += $(patsubst %.c,%.o,$(wildcard arch/${ARCH}/*/*.c))

# Assembly sources only come from the arch-dependent directory
ARCH_ASMOBJS  = $(patsubst %.S,%.o,$(wildcard arch/${ARCH}/*.S))
ARCH_ASMOBJS += $(patsubst %.asm,%.o,$(wildcard arch/${ARCH}/*.asm))

# Kernel modules are one file = one module; if you want to build more complicated
# modules, you could potentially use `ld -r` to turn multiple source objects into
# a single relocatable object file.
ARCH_ENABLED_MODS = $(shell util/valid-modules.sh $(ARCH))
MODULES = $(patsubst modules/%.c,$(BASE)/mod/%.ko,$(foreach mod,$(ARCH_ENABLED_MODS),modules/$(mod).c))

EMU = qemu-system-${ARCH}

APPS=$(patsubst apps/%.c,%,$(wildcard apps/*.c))
APPS_X=$(foreach app,$(APPS),$(BASE)/bin/$(app))
APPS_Y=$(foreach app,$(APPS),.make/$(app).mak)
APPS_SH=$(patsubst apps/%.sh,%.sh,$(wildcard apps/*.sh))
APPS_SH_X=$(foreach app,$(APPS_SH),$(BASE)/bin/$(app))
# FIXME
APPS_KRK=$(patsubst apps/%.krk,%.krk,$(wildcard apps/*.krk))
APPS_KRK_X=$(foreach app,$(APPS_KRK),$(BASE)/bin/$(app))

LIBS=$(patsubst lib/%.c,%,$(wildcard lib/*.c))
LIBS_X=$(foreach lib,$(LIBS),$(BASE)/lib/libtoaru_$(lib).so)
LIBS_Y=$(foreach lib,$(LIBS),.make/$(lib).lmak)

# FIXME
KRK_MODS = $(patsubst kuroko/src/modules/module_%.c,$(BASE)/lib/kuroko/%.so,$(wildcard kuroko/src/modules/module_*.c))
KRK_MODS += $(patsubst kuroko/modules/%,$(BASE)/lib/kuroko/%,$(wildcard kuroko/modules/*.krk kuroko/modules/*/*/.krk kuroko/modules/*/*/*.krk))
KRK_MODS_X = $(patsubst lib/kuroko/%.c,$(BASE)/lib/kuroko/%.so,$(wildcard lib/kuroko/*.c))
KRK_MODS_Y = $(patsubst lib/kuroko/%.c,.make/%.kmak,$(wildcard lib/kuroko/*.c))

# CFLAGS for user mode
CFLAGS  = -O2 -g
CFLAGS += -Wall -Wextra -Wno-unused-parameter
CFLAGS += -I. -Iapps
CFLAGS += -fplan9-extensions ${ARCH_USER_CFLAGS}

LIBC_OBJS  = $(patsubst %.c,%.o,$(wildcard libc/*.c))
LIBC_OBJS += $(patsubst %.c,%.o,$(wildcard libc/*/*.c))
LIBC_OBJS += $(patsubst %.c,%.o,$(wildcard libc/arch/${ARCH}/*.c))

GCC_SHARED = $(BASE)/usr/lib/libgcc_s.so.1 $(BASE)/usr/lib/libgcc_s.so

CRTS = $(BASE)/lib/crt0.o $(BASE)/lib/crti.o $(BASE)/lib/crtn.o

LIBC = $(BASE)/lib/libc.so $(GCC_SHARED)

PHONY += all system clean run shell

all: qemu-kernel

PHONY += qemu-kernel

qemu-kernel: hos.kernel
	qemu-system-i386 -kernel $(KERNEL_BUILD_DIR)/kernel_multiboot.elf
	# qemu-system-i386 -kernel $(KERNEL_BUILD_DIR)/kernel_multiboot.elf -s -S

# .PHONY: $(PHONY)

# FIXME
$(BASE)/mod/%.ko: modules/%.c | dirs
	${CC} -c ${K_CFLAGS} -mcmodel=large  -o $@ $<

# FIXME
ramdisk.igz: $(wildcard $(BASE)/* $(BASE)/*/* $(BASE)/*/*/* $(BASE)/*/*/*/* $(BASE)/*/*/*/*/*) $(APPS_X) $(LIBS_X) $(KRK_MODS_X) $(BASE)/bin/kuroko $(BASE)/lib/ld.so $(BASE)/lib/libm.so $(APPS_KRK_X) $(KRK_MODS) $(APPS_SH_X) $(MODULES)
	python3 util/createramdisk.py

# FIXME
KRK_SRC = $(sort $(wildcard kuroko/src/*.c))
$(BASE)/bin/kuroko: $(KRK_SRC) $(CRTS)  lib/rline.c | $(LIBC)
	$(CC) $(CFLAGS) -o $@ -Wl,--export-dynamic -Ikuroko/src $(KRK_SRC) lib/rline.c

# FIXME
$(BASE)/lib/kuroko/%.so: kuroko/src/modules/module_%.c| dirs $(LIBC)
	$(CC) $(CFLAGS) -shared -fPIC -Ikuroko/src -o $@ $<

# FIXME
$(BASE)/lib/kuroko/%.krk: kuroko/modules/%.krk | dirs
	mkdir -p $(dir $@)
	cp $< $@

# FIXME
$(BASE)/lib/libkuroko.so: $(KRK_SRC) | $(LIBC)
	$(CC) -O2 -shared -fPIC -Ikuroko/src -o $@ $(filter-out kuroko/src/kuroko.c,$(KRK_SRC))

$(BASE)/lib/ld.so: linker/linker.c $(BASE)/lib/libc.a | dirs $(LIBC)
	$(CC) -g -static -Wl,-static $(CFLAGS) -z max-page-size=0x1000 -o $@ -Os -T linker/link.ld $<

hos.kernel: arch/$(ARCH)/linker.ld $(KERNEL_OBJS) $(ARCH_OBJS) $(ARCH_OBJS)
	$(CC) -T arch/$(ARCH)/linker.ld $(K_CFLAGS) -o $@ $(filter-out arch/$(ARCH)/linker.ld, $^)

kernel/sys/version.o: ${KERNEL_SRCS}

# FIXME
kernel/symbols.o: ${KERNEL_ASMOBJS} ${KERNEL_OBJS} util/gensym.krk
	-rm -f kernel/symbols.o
	${CC} -T arch/${ARCH}/linker.ld ${K_CFLAGS} -o misaka-kernel.64 ${KERNEL_ASMOBJS} ${KERNEL_OBJS}
	${NM} misaka-kernel.64 -g | kuroko util/gensym.krk > kernel/symbols.S
	${CC} -c kernel/symbols.S -o $@

kernel/%.o: kernel/%.S
	${CC} $(K_CFLAGS) -c $< -o $@ 

arch/%.o: arch/%.S
	${CC} $(K_CFLAGS) -c $< -o $@ 

KERNEL_HDRS  = $(wildcard include/kernel/*.h)
KERNEL_HDRS += $(wildcard include/kernel/*/*.h)

HEADERS = $(wildcard include/*.h)

kernel/%.o: kernel/%.c ${KERNEL_HDRS}
	${CC} ${K_CFLAGS} -nostdlib -Iinclude -c -o $@ $<

arch/%.o: arch/%.c ${KERNEL_HDRS}
	${CC} ${K_CFLAGS} -nostdlib -Iinclude -c -o $@ $<

clean:
	-rm -f ${KERNEL_ASMOBJS}
	-rm -f ${KERNEL_OBJS} $(MODULES)
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

libc/%.o: libc/%.c base/usr/include/syscall.h 
	$(CC) -O2 -std=gnu11 -ffreestanding -Wall -Wextra -Wno-unused-parameter -fPIC -c -o $@ $<

PHONY += libc

libc: $(BASE)/lib/libc.a $(BASE)/lib/libc.so

$(BASE)/lib/libc.a: ${LIBC_OBJS} $(CRTS)
	$(AR) cr $@ $(LIBC_OBJS)

$(BASE)/lib/libc.so: ${LIBC_OBJS} | $(CRTS)
	${CC} -nodefaultlibs -shared -fPIC -o $@ $^ -lgcc

$(BASE)/lib/crt%.o: libc/arch/${ARCH}/crt%.S
	${AS} -o $@ $<

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
-include ${KRK_MODS_Y}
endif

# FIXME
.make/%.lmak: lib/%.c util/auto-dep.krk | dirs $(CRTS)
	kuroko util/auto-dep.krk --makelib $< > $@

.make/%.mak: apps/%.c util/auto-dep.krk | dirs $(CRTS)
	kuroko util/auto-dep.krk --make $< > $@

.make/%.kmak: lib/kuroko/%.c util/auto-dep.krk | dirs
	kuroko util/auto-dep.krk --makekurokomod $< > $@

$(BASE)/bin/%.sh: apps/%.sh
	cp $< $@
	chmod +x $@

$(BASE)/bin/%.krk: apps/%.krk
	cp $< $@
	chmod +x $@

PHONY += libs
libs: $(LIBS_X)

PHONY += apps
apps: $(APPS_X)

SOURCE_FILES  = $(wildcard kernel/*.c kernel/*/*.c kernel/*/*/*.c kernel/*/*/*/*.c)
SOURCE_FILES += $(wildcard apps/*.c linker/*.c libc/*.c libc/*/*.c lib/*.c lib/kuroko/*.c)
# FIXME
SOURCE_FILES += $(wildcard kuroko/src/*.c kuroko/src/*.h kuroko/src/*/*.c kuroko/src/*/*.h)
SOURCE_FILES += $(wildcard $(BASE)/usr/include/*.h $(BASE)/usr/include/*/*.h $(BASE)/usr/include/*/*/*.h)

tags: $(SOURCE_FILES)
	ctags -f tags $(SOURCE_FILES)

.PHONY: $(PHONY)
