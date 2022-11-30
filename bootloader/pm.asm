bits 16

switch_pm:
    cli				; clear interrupts
	mov	eax, cr0		; set bit 0 in cr0--enter pmode
	or	eax, 1
	mov	cr0, eax

    ret

bits 32

init_pm_env: ; we are now using 32-bit instructions
    pusha

    mov ax, DATA_SEG ; 5. update the segment registers
    mov ds, ax
    mov ss, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    popa

    mov ebp, 0x90000 ; 6. update the stack right at the top of the free space
    mov esp, ebp

    ret

