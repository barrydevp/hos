bits 32					; Welcome to the 32 bit world!
 
boot3:
 
	;-------------------------------;
	;   Set registers		;
	;-------------------------------;
 
	mov		ax, 0x10		; set data segments to data selector (0x10)
	mov		ds, ax
	mov		ss, ax
	mov		es, ax
	mov		esp, 90000h		; stack begins from 90000h


    ; print hello
    mov si, msg
    call print

    ; mov cursor to next line
    inc dh
    call mov_cursor

    ; print log mem title
    mov si, msg1
    call print

    ; get memory size
    xor ax, ax
    int 0x12

    ; print memory size
    xor ecx, ecx
    mov cx, ax
    call print_num

	cli			; Clear all Interrupts
	hlt			; halt the system

