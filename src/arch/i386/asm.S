.set NULL_SEGMENT, 0x0
.set CODE_SEGMENT, 0x08
.set DATA_SEGMENT, 0x10

.section .text

.global gdt_load   // Allows the C code to call gdt_flush().
gdt_load:
	movl 4(%esp), %eax // Get the pointer to the GDT, passed as a parameter.
	lgdt (%eax)        // Load the new GDT pointer.

	mov $DATA_SEGMENT, %ax    // 0x10 is the offset in the GDT to our data segment.
	mov %ax, %ds      // Load all data segment selectors.
	mov %ax, %es
	mov %ax, %fs
	mov %ax, %gs
	mov %ax, %ss
	jmp $CODE_SEGMENT, $_flush // Far jump to the offset 0x08 (code segment).
_flush:
	ret

.global idt_load    // Allows the C code to call idt_flush(/.
idt_load:
	movl 4(%esp), %eax // Get the pointer to the IDT, passed as a parameter.
	lidt (%eax)        // Load the IDT pointer.
	ret


.global tss_load    // Allows our C code to call tss_flush().
tss_load:
	mov $0x2B, %ax    // Load the index of our TSS structure.
										 // The index is 0x28, as it is the 5th selector and each is 8 bytes
										 // long, but we set the bottom two bits (making 0x2B)
										 // so that it has an RPL of 3, not zero.
	ltr %ax            // Load 0x2B into the task state register.
	ret

