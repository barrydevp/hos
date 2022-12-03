;******************************************
; second.asm		
; Second bootloader
; Objective: Load env, kernel, os
;******************************************

bits    16
org	0x500						; We are loaded by first bootloader at 0x50:0x0

; constants directive
bpbBytesPerSector   EQU 512
SEC_BOOT_ADDR       EQU 0x50
IMAGE_RMODE_BASE    EQU 0x3000
IMAGE_PMODE_BASE    EQU 0x100000
N_IMAGE_SECTORS     EQU 20

; real code start from here

; entry point from first boot
start2:          jmp boot2

; data
LOAD_GDT_MSG        db      "Load GDT", 0x0D, 0x0A, 0x00 
ENABLE_A20_MSG      db      "Enable Address 20", 0x0D, 0x0A, 0x00 
SWITCH_PM_MSG       db      "Switch Protected Mode", 0x0D, 0x0A, 0x00 
LOAD_IMAGE_RM_MSG   db      "Load image RM", 0x0D, 0x0A, 0x00 
LOAD_IMAGE_PM_MSG   db      "Load image PM", 0x0D, 0x0A, 0x00 

%include "inc/io16.asm"
%include "inc/gdt.asm"
%include "inc/a20.asm"

;*************************************************;
;	Bootloader Entry Point
;*************************************************;

; bits    16          ; important because some of these included file above will end up with 32 bit mode
boot2:
    ; setup
    ; xor	ax, ax		; Setup segments to insure they are 0. Remember that
    ; mov	ds, ax		; we have ORG 0x1000 due to previous boot stage we read second.asm code into it. 
    ; mov	es, ax      ; This means all addresses are based from 0x1000:0. 

    ;-------------------------------;
    ;   Setup segments and stack	;
    ;-------------------------------;

    cli				; clear interrupts
    xor ax, ax		; Setup segments to insure they are 0. Remember that
    mov ds, ax		; we have ORG 0x1000 due to previous boot stage we read second.asm code into it. 
    mov es, ax      ; This means all addresses are based from 0x1000:0. 
    mov ax, 0x9000		; stack begins at 0x9000-0xffff
    mov ss, ax
    mov sp, 0xFFFF
    sti				; enable interrupts   


    ;-------------------------------;
    ;   Install our GDT		;
    ;-------------------------------;

    mov si, LOAD_GDT_MSG
    call print_16

    cli ; 1. disable interrupts
    lgdt [gdt_descriptor] ; 2. load the GDT descriptor
    sti         ; enable interrupts

    ;-------------------------------;
    ;   Enable A20                  ;
    ;-------------------------------;

    mov si, ENABLE_A20_MSG
    call print_16

    call EnableA20_KKbrd_Out

    ;-------------------------------;
    ; Load Kernel		    ;
    ;-------------------------------;
    
    mov si, LOAD_IMAGE_RM_MSG
    call print_16

__load_image:
    ; prepare for read, reset buffer
    xor ax, ax
    mov es, ax
    mov bx, IMAGE_RMODE_BASE ; we are going to read image into memory address from 0:IMAGE_RMODE_BASE(ES:BX)

__load_image_sector:
    ; read n sectors (512 bytes each)
    mov al, N_IMAGE_SECTORS ; number of image's sectors (size)
    mov ch, 0 ; track to read
    mov cl, 3 ; start read from sector (1: stage1(512 byte), 2: stage2(512 byte), 3:other)
    mov dh, 0 ; head number (18 sector/head)
    mov dl, 0 ; drive number (0 mean floppy disk)
    mov ah, 0x02 ; set function to call of interrupt
    int 0x13
    jc  __load_image_sector ; retry if err

    ;-------------------------------;
    ;   Go into pmode		;
    ;-------------------------------;

    mov si, SWITCH_PM_MSG
    call print_16

    cli				; clear interrupts
    mov	eax, cr0		; set bit 0 in cr0--enter pmode
    or	eax, 1
    mov	cr0, eax

__on_pmode:

    ;-------------------------------;
    ;   We are in pmode             ;
    ;-------------------------------;

    jmp	CODE_DESC:__setup		; far jump to fix CS. Remember that the code selector is 0x8!

    ; Note: Do NOT re-enable interrupts! Doing so will triple fault!
    ; We will fix this in Stage 3.
 

bits 32					; Welcome to the 32 bit world!
 
__setup:
    ;-------------------------------;
    ;   Set registers		;
    ;-------------------------------;
 
    mov ax, DATA_DESC ; 5. update the segment registers, set data segments to data selector (0x10)
    mov ds, ax
    mov ss, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ebp, 0x90000 ; 6. update the stack right at the top of the free space, stack begins from 0x90000
    mov esp, ebp

    
    ;-------------------------------;
    ; Copy kernel to 1MB	    ;
    ;-------------------------------;

    mov eax, N_IMAGE_SECTORS ;n image's sectors
    mov ebx, bpbBytesPerSector
    mul ebx
    mov ebx, 4
    div ebx
    cld
    mov esi, IMAGE_RMODE_BASE
    mov edi, IMAGE_PMODE_BASE
    mov ecx, eax
    rep movsd                   ; copy image to its protected mode address

    ;---------------------------------------;
    ;   Execute Kernel			    ;
    ;---------------------------------------;

    jmp CODE_DESC:IMAGE_PMODE_BASE; jump to our kernel! Note: This assumes Kernel's entry point is at 1 MB
    ; jmp [IMAGE_PMODE_BASE + 18h]; jump to our kernel (at address of elf entry)! Note: This assumes Kernel's entry point is at 1 MB

__debug:
; %include "inc/io32.asm"
;     call clrscr_32
;
;     mov bh, 3
;     mov bl, 4
;     call mov_cursor_32
;
;     ; print hello
;     mov ebx, LOAD_IMAGE_PM_MSG
;     call print_32 ; Note that this will be written at the top left corner;


    ; ; mov cursor to next line
    ; inc dh
    ; call mov_cursor
    ;
    ; ; print log mem title
    ; mov si, msg1
    ; call print
    ;
    ; ; get memory size
    ; xor ax, ax
    ; int 0x12
    ;
    ; ; print memory size
    ; xor ecx, ecx
    ; mov cx, ax
    ; call print_num

halt:
    cli			; Clear all Interrupts
    hlt			; halt the system

