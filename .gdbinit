define hook-stop
    # Translate the segment:offset into a physical address
    printf "[%4x:%4x] ", $cs, $eip
end
set architecture i8086
# set architecture i386:intel
directory ./kernel
layout asm
layout reg
set disassembly-flavor intel
# target remote localhost:26000
target remote localhost:1234
# symbol-file ./.build/kernel/kernel_boot.elf
symbol-file ./.build/kernel/kernel_multiboot.elf
# b *0x7c00
# b main
