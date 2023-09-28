/* Macros */
.macro IRQ index byte
    .global irq\index
    .type irq\index, @function
    irq\index:
        push $0x00
        push $\byte
        jmp isr_common
.endm

.macro ISR_NOERR index
    .global isr\index
    .type isr\index, @function
    isr\index:
        push $0x00
        push $\index
        jmp isr_common
.endm

.macro ISR_ERR index
    .global isr\index
    .type isr\index, @function
    isr\index:
        push $\index
        jmp isr_common
.endm

/* Code */
.section .text

.extern isr_handler

/* Common ISR code */
isr_common:
    /* 1. Save CPU state (registers) */
    pushal // Pushes edi,esi,ebp,esp,ebx,edx,ecx,eax

    push %ds
    push %es
    push %fs
    push %gs
    mov $0x10, %ax  // kernel data segment descriptor
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs

    /* 2. Call interrupt handler */
    cld // C code following the sysV ABI requires DF to be clear on function entry
    push %esp // interrupt_registers *r
    call isr_handler
    add $4, %esp 
    
    /* 3. Restore state (registers) */
    pop %gs
    pop %fs
    pop %es
    pop %ds
    
    popal // reverse of pushad

    /* clean up error code an interrupt */
    add $8, %esp  // Cleans up the pushed error code and pushed ISR number
		
    /* return from interrupt */
    iret // pops 5 things at once: CS, EIP, EFLAGS, SS, and ESP


/** We don't get information about which interrupt was caller
  * when the handler is run, so we will need to have a different handler
  * for every interrupt.
  * Furthermore, some interrupts push an error code onto the stack but others
  * don't, so we will push a dummy error code for those which don't, so that
  * we have a consistent stack for all of them.
  */

/* Interrupt Requests */
ISR_NOERR 0
ISR_NOERR 1
ISR_NOERR 2
ISR_NOERR 3
ISR_NOERR 4
ISR_NOERR 5
ISR_NOERR 6
ISR_NOERR 7
ISR_ERR   8
ISR_NOERR 9
ISR_ERR   10
ISR_ERR   11
ISR_ERR   12
ISR_ERR   13
ISR_ERR   14
ISR_NOERR 15
ISR_NOERR 16
ISR_NOERR 17
ISR_NOERR 18
ISR_NOERR 19
ISR_NOERR 20
ISR_NOERR 21
ISR_NOERR 22
ISR_NOERR 23
ISR_NOERR 24
ISR_NOERR 25
ISR_NOERR 26
ISR_NOERR 27
ISR_NOERR 28
ISR_NOERR 29
ISR_NOERR 30
ISR_NOERR 31

// IRQ
IRQ 0, 32
IRQ 1, 33
IRQ 2, 34
IRQ 3, 35
IRQ 4, 36
IRQ 5, 37
IRQ 6, 38
IRQ 7, 39
IRQ 8, 40
IRQ 9, 41
IRQ 10, 42
IRQ 11, 43
IRQ 12, 44
IRQ 13, 45
IRQ 14, 46
IRQ 15, 47

/* syscall entry point */
ISR_NOERR 127
ISR_NOERR 128
