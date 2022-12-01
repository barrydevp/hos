; the first bootloader stage

; %include "stage1.asm"

; reset floppy disk
; __reset_floppy:
;     mov dl, 0
;     mov ah, 0x00
;     int 0x13
;     jc  __reset_floppy
;
;     ; prepare for read, reset buffer
;     mov ax, SEC_BOOT_ADDR ; we are going to read sector into memory address from 0x1000:0
;     mov es, ax
;     xor bx, bx
;
; __load_sec_boot:
;     ; read n sectors (512 bytes each)
;     mov al, 10 ; read n sectors
;     mov ch, 0 ; track to read
;     mov cl, 2 ; start read from sector
;     mov dh, 0 ; head number
;     mov dl, 0 ; drive number (0 mean floppy disk)
;     mov ah, 0x02 ; set function to call of interrupt
;     int 0x13
;     jc  __load_sec_boot ; retry if err
;
;     jmp SEC_BOOT_ADDR:0x0 ; jump to start of new instructions, second boot
