org 0x100000			; Kernel starts at 1 MB

bits 32				; 32 bit code

; times 512 - ($-$$) db 1		; We have to be 512 bytes. Clear the rest of the bytes with 0

jmp main				; jump to entry point

%include "inc/io32.asm"

HELLO_MSG	db    "Hello World!", 0x0D, 0x0A, 0x00 

main:
    call clrscr_32

    mov bh, 3
    mov bl, 4
    call mov_cursor_32

    ; print hello
    mov ebx, HELLO_MSG
    call print_32 ; Note that this will be written at the top left corner;


halt:
    cli
    hlt
