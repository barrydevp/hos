bits 32 ;using 32-bit protected mode

; this is how constants are defined
VIDEO_MEMORY        equ     0xb8000 ; video start memory
WHITE_ON_BLACK      equ     0x0f    ; the color byte for each character
COLS                equ     80      ; width and height of screen
LINES               equ     25


; location cursor
_CurX       db      0
_CurY       db      0


; procedures

print_32:
    pusha
    mov edx, VIDEO_MEMORY

print_32__loop:
    mov al, [ebx] ; [ebx] is the address of our character
    mov ah, WHITE_ON_BLACK

    cmp al, 0 ; check if end of string
    je print_32__r

    mov [edx], ax ; store character + attribute in video memory
    add ebx, 1 ; next char
    add edx, 2 ; next video memory position

    jmp print_32__loop

print_32__r:
    popa
    ret


;**************************************************;
;	GotoXY ()
;		- Set current X/Y location
;	parm\	AL=X position
;	parm\	AH=Y position
;**************************************************;

goto_xy_32:
    pusha
    mov[_CurX], al; just set the current position
    mov[_CurY], ah
    popa
    ret


;**************************************************;
;	MoveCur ()
;		- Update hardware cursor
;	parm/ bh = Y pos
;	parm/ bl = x pos
;**************************************************;

mov_cursor_32:

    pusha				; save registers (aren't you getting tired of this comment?)

    ;-------------------------------;
    ;   Get current position        ;
    ;-------------------------------;

    ; Here, _CurX and _CurY are relitave to the current position on screen, not in memory.
    ; That is, we don't need to worry about the byte alignment we do when displaying characters,
    ; so just follow the forumla: location = _CurX + _CurY * COLS

    xor	eax, eax
    mov	ecx, COLS
    mov	al, bh			; get y pos
    mul	ecx			; multiply y*COLS
    add	al, bl			; Now add x
    mov	ebx, eax

    ;--------------------------------------;
    ;   Set low byte index to VGA register ;
    ;--------------------------------------;

    mov	al, 0x0f
    mov	dx, 0x03D4
    out	dx, al

    mov	al, bl
    mov	dx, 0x03D5
    out	dx, al			; low byte

    ;---------------------------------------;
    ;   Set high byte index to VGA register ;
    ;---------------------------------------;

    xor	eax, eax

    mov	al, 0x0e
    mov	dx, 0x03D4
    out	dx, al

    mov	al, bh
    mov	dx, 0x03D5
    out	dx, al			; high byte

    popa
    ret


;**************************************************;
;ClrScr32 ()
;- Clears screen
;**************************************************;

clrscr_32:

    pusha
    cld
    mov	edi, VIDEO_MEMORY
    mov	cx, 2000
    mov	ah, WHITE_ON_BLACK
    mov	al, ' '	
    rep	stosw

    mov	byte [_CurX], 0
    mov	byte [_CurY], 0

    popa
    ret


