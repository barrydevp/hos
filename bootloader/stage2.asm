;******************************************
; second.asm		
; Second bootloader
; Objective: Load env, kernel, os
;******************************************

bits    16
org	0x500						; We are loaded by first bootloader at 0x50:0x0

; constants directive
SEC_BOOT_ADDR EQU 0x50

; real code start from here

; entry point from first boot
start2:          jmp boot2

; data
HELLO_MSG	db      "Hello World!", 0x0D, 0x0A, 0x00 
msg1		db      "Memory Size: ", 0 
msg2		db      "@", 0
LOAD_GDT_MSG        db      "Loading GDT...", 0x0D, 0x0A, 0x00 
ENABLE_A20_MSG      db      "Enabling Address 20...", 0x0D, 0x0A, 0x00 
SWITCH_PM_MSG       db      "Switching to Protected Mode...", 0x0D, 0x0A, 0x00 

%include "io16.asm"
%include "io32.asm"
%include "gdt.asm"
%include "a20.asm"

;*************************************************;
;	Bootloader Entry Point
;*************************************************;

bits    16          ; important because some of these included file above will end up with 32 bit mode
boot2:
    ; setup
    ; xor	ax, ax		; Setup segments to insure they are 0. Remember that
    ; mov	ds, ax		; we have ORG 0x1000 due to previous boot stage we read second.asm code into it. 
    ; mov	es, ax      ; This means all addresses are based from 0x1000:0. 

    ; for screen manipulating, 'bh' register should be 0 (display page number)
    ; position cursor
    ; mov dh, 0x5
    ; mov dl, 0x8
    ; call mov_cursor

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

    jmp	CODE_SEG:__setup		; far jump to fix CS. Remember that the code selector is 0x8!

    ; Note: Do NOT re-enable interrupts! Doing so will triple fault!
    ; We will fix this in Stage 3.
 

bits 32					; Welcome to the 32 bit world!
 
__setup:
    ;-------------------------------;
    ;   Set registers		;
    ;-------------------------------;
 
    mov ax, DATA_SEG ; 5. update the segment registers, set data segments to data selector (0x10)
    mov ds, ax
    mov ss, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ebp, 0x90000 ; 6. update the stack right at the top of the free space, stack begins from 0x90000
    mov esp, ebp

    

main:
    call clrscr_32

    mov bh, 3
    mov bl, 4
    call mov_cursor_32
    
    ; print hello
    mov ebx, HELLO_MSG
    call print_32 ; Note that this will be written at the top left corner;


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

