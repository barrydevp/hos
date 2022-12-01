;*********************************************
;	
;		- A Simple Bootloader
;
;	Operating Systems Development Tutorial
;*********************************************

bits	16							; We are still in 16 bit Real Mode

org		0x7c00						; We are loaded by BIOS at 0x7C00

; constants directive
SEC_BOOT_ADDR EQU 0x50

; real code start from here
start:          jmp boot					; jump over OEM block

;*************************************************;
;	OEM Parameter block
;*************************************************;

; TIMES 0Bh-$+start DB 0
;
; bpbBytesPerSector:  	DW 512
; bpbSectorsPerCluster: 	DB 1
; bpbReservedSectors: 	DW 1
; bpbNumberOfFATs: 	    DB 2
; bpbRootEntries: 	    DW 224
; bpbTotalSectors: 	    DW 2880
; bpbMedia: 	            DB 0xF0
; bpbSectorsPerFAT: 	    DW 9
; bpbSectorsPerTrack: 	DW 18
; bpbHeadsPerCylinder: 	DW 2
; bpbHiddenSectors: 	    DD 0
; bpbTotalSectorsBig:     DD 0
; bsDriveNumber: 	        DB 0
; bsUnused: 	            DB 0
; bsExtBootSignature: 	DB 0x29
; bsSerialNumber:	        DD 0xa0a1a2a3
; bsVolumeLabel: 	        DB "MOS FLOPPY "
; bsFileSystem: 	        DB "FAT12   "



boot:
    cli			; clear interrupts
    xor	ax, ax		; Setup segments to insure they are 0. Remember that
    mov	ds, ax		; we have ORG 0x7c00. This means all addresses are based
    mov	es, ax		; from 0x7c00:0. Because the data segments are within the same

    ; reset floppy disk
__reset_floppy:
    mov dl, 0
    mov ah, 0x00
    int 0x13
    jc  __reset_floppy

    ; prepare for read, reset buffer
    mov ax, SEC_BOOT_ADDR ; we are going to read sector into memory address from SEC_BOOT_ADDR:0
    mov es, ax
    xor bx, bx

__load_sec_boot:
    ; read n sectors (512 bytes each)
    mov al, 1 ; read n sectors
    mov ch, 0 ; track to read
    mov cl, 2 ; start read from sector
    mov dh, 0 ; head number
    mov dl, 0 ; drive number (0 mean floppy disk)
    mov ah, 0x02 ; set function to call of interrupt
    int 0x13
    jc  __load_sec_boot ; retry if err

    jmp SEC_BOOT_ADDR:0x0 ; jump to start of new instructions, second boot
	
times 510 - ($-$$) db 0		; We have to be 512 bytes. Clear the rest of the bytes with 0

dw 0xAA55			; Boot Signiture
