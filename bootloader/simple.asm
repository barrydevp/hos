org 0x7c00
bits 16
start: jmp boot

msg db "Hello world!", 0ah, 0ah, 0h


boot:
    cli ; no interrupts
    cld ; all that we need to init
    hlt ; halt the system

times 510 - ($-$$) db 0

dw 0xAA55 ; Boot signature
