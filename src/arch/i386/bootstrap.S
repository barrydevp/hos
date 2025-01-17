# Declare a multiboot header that marks the program as a kernel.
.section .multiboot.data, "aw"

/* global constant */
.set KERNEL_HIGHER_HALF, 0xC0000000

/* Multiboot 2 header */
.set MB2_MAGIC,    0xe85250d6
.set MB2_ARCH,     0
.set MB2_LENGTH,   (multiboot2_header_end - multiboot2_header)
.set MB2_CHECKSUM, -(MB2_MAGIC + MB2_ARCH + MB2_LENGTH)
.align 8
multiboot2_header:
.long MB2_MAGIC
.long MB2_ARCH
.long MB2_LENGTH
.long MB2_CHECKSUM

/* Address tag */
.align 8
mb2_tag_addr:
.word 2
.word 1
.long 24
.long multiboot2_header
.long _bootloader_start           /* load_addr */
.long bss_start - 0xC0000000      /* load_end_addr */
.long _kernel_end - 0xC0000000    /* bss_end_addr */

/* Entry tag */
#.align 8
mb2_tag_entry:
.word 3
#.word 0
.word 1
.long 12
.long _start

/* Flags tag */
.align 8
.word 4
.word 1
.long 12
.long 2 /* We support EGA text, but don't require it */

/* Framebuffer tag */
.align 8
mb2_tag_fb:
.word 5
#.word 0
.word 1
.long 20
# .long 1024
# .long 768
# .long 16
.long 640
.long 480
.long 16
# .long 80
# .long 25
# .long 16
mb2_tag_fb_end:

/* Modules alignment tag */
.align 8
.word 6
.word 0
.long 8

/* End tag */
.align 8
.word 0
.word 0
.long 8

multiboot2_header_end:

# Allocate the initial stack.
.section .kernel_stack, "aw", @nobits
.global _kernel_stack_bottom
_kernel_stack_bottom:
.skip 16384 # 16 KiB
.global _kernel_stack_top
_kernel_stack_top:

# The kernel entry point.
.section .multiboot.text, "a"
.global _start
.type _start, @function
_start:
	# Set up the stack. Since paging is not enable yet
	# we need to use physical address of the kernel stack
	# and current _kernel_stack_top is virtual address
	# thus, we have to minus KERNEL_HIGHER_HALF to get the physical one
	mov $(_kernel_stack_top - KERNEL_HIGHER_HALF), %esp
	/* Make sure our stack is 16-byte aligned */
	and $-16, %esp

	/* Reset EFLAGS. */
	pushl $0
	popf

	# push multiboot info
	pushl %ebx /* Multiboot header pointer */
	pushl %eax /* Multiboot header magic */


_setup_paging:
	.extern init_page_region // located at: src/kernel/memory/vmm.c
	/* Set up initial page region, which was zero'd for us by the loader */
	// we need to minus KERNEL_HIGHER_HALF since paging is not enable
	// and init_page_region is virtual address
	mov $(init_page_region - KERNEL_HIGHER_HALF), %edi

	/* PDT[0] = &PT[0] | (PRESENT, WRITABLE, KERNEL) */
	/*        = init_page_region[0] | ... */
	// notice flag is 0x3 and remain 0x1000 is offset to the PT[0] pointer
	// which can be break as: mov 0x03, eax; add 0x1000, eax
	// first 4MB identity map, https://wiki.osdev.org/Identity_Paging
	// where we map exactly first 4MB of physical address to 4MB of virtual address
	mov $0x1003, %eax  
	add %edi, %eax
	mov %eax, (%edi)

	/* PDT[768] = &PT[0] | (PRESENT, WRITABLE, KERNEL) */
	/*          = init_page_region[0] | ... */
	// kernel higher half mapping (remember size of each entry = 4bytes)
	// first 4MB from kernel higher half virtual address also refer to first 4MB of physical address
	add $768 * 4, %edi
	mov %eax, (%edi)

	/* PDT[769] = &PT[1] | (PRESENT, WRITABLE, KERNEL) */
	/*          = init_page_region[1] | ... */
	// Next 4MB (boot + kernel code)
	add $4, %edi
	add $0x1000, %eax
	mov %eax, (%edi)

	/* PDT[769] = &PT[1] | (PRESENT, WRITABLE, KERNEL) */
	// next 4MB identity map (this will contain the multiboot + kernel)
	sub $768 * 4, %edi
	mov %eax, (%edi)

	/* Setup 8MB mapping(1MB) + multiboot + kernel to PDT[0] */
	// FIXME: if the kernel size is largen than 3MB, need more PT
	// FIXME!: Remeber the size of frams_bitmap for 4GB mem is 
	// 1 = because last edi is PDT[1] so we add 1023 * 4 to move it
	// to PT[0]
	add $0x1000 - 1 * 4, %edi
	mov $0x0003, %ebx
	mov $2048, %ecx

_set_entry:
	mov %ebx, (%edi)
	add $0x1000, %ebx
	add $4, %edi
	loop _set_entry

_en_paging:
	# Set cr3 to the address of the page_directory.
	mov $(init_page_region - KERNEL_HIGHER_HALF), %edi
	mov %edi, %cr3

	# Set PSE bit in CR4 to enable 4MB pages
	# mov %cr4, %ecx
	# or $0x00000010, %ecx
	# mov %ecx, %cr4

	# Enable paging and the write-protect bit.
	mov %cr0, %ecx
	or $0x80010000, %ecx
	mov %ecx, %cr0

_higher_half_enter:
	# Jump to higher half with an absolute jump. 
	lea start_hihger_half, %ecx
	jmp *%ecx

.section .text
start_hihger_half:
	.extern kenter
	.type kenter, @function
	# At this point, paging is fully set up and enabled.

	# Unmap the identity mapping as it is now unnecessary. 
	# movl $0, boot_page_directory + 0

	# Reload crc3 to force a TLB flush so the changes to take effect.
	movl %cr3, %ecx
	movl %ecx, %cr3

	# We set the stack back to higher half 
	mov %esp, %eax
	add $KERNEL_HIGHER_HALF, %eax
	mov %eax, %esp

	# Enter the high-level kernel.
	# at this point, multiboot magic and pointer has already pushed
	# when first jump to _start, so free to enter the kernel entry
	# call kenerl entry point
	call kenter

	# Infinite loop if the system has nothing more to do.
	cli
1:
	hlt
	jmp 1b
