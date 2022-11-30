bits	16

; io utility

;; mov_cursor_16() {
mov_cursor_16:
    mov ah, 0x02
    int 0x10

    ret
;; }


;; putc() {
putc_16:
    mov ah, 0x0e
    int 0x10

    ret
;; }

debug_16:
    mov al, '@'
    mov ah, 0x0e
    int 0x10
    

;; print_16() {
print_16:
    mov ah, 0x0e

print_16__loop:
    lodsb ; equivalent to: mov al, [esi]
          ;                inc esi
    or  al, al
    jz  print_16__r
    int 0x10
    jmp print_16__loop

print_16__r:
    ret
;; }

;; print_16_num() {
divisor_tbl:
    dd 1000000000
    dd 100000000
    dd 10000000
    dd 1000000
    dd 100000
    dd 10000
    dd 1000
    dd 100
    dd 10
    dd 1
    dd 0
print_16_num:
    ; check if zero, print_16 and return
    or  ecx, ecx
    jz  print_16_num__izero
    mov ebx, divisor_tbl

print_16_num__loop:
    ; ; if ecx == 0 {
    ; ;   print_16_num__r
    ; or  ecx, ecx
    ; jz  print_16_num__r
    ; ; }
    
    xor edx, edx
    mov eax, ecx

    div dword [ebx]

    mov ecx, edx
    add ebx, 4

    or eax, eax
    jz print_16_num__loop

    add al, '0'
    call putc_16

    mov eax, [ebx]
    or  eax, eax
    jz  print_16_num__r

    jmp print_16_num__loop

print_16_num__izero:
    mov al, '0'
    call putc_16

print_16_num__r:
    ret
;; }
