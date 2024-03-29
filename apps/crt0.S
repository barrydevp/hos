.extern main
.extern __libc_start_main
.global _start

# -----------------------------------------------------------------------------
# SECTION (text)
# -----------------------------------------------------------------------------
.section .text
_start:                     # _start is the entry point known to the linker
    mov $0, %ebp            # Set ebp to 0 as x86 programs require
    push $main               # Push the pointer to `main` to the stack.
    call __libc_start_main  # Call the libc initialization function.
    mov %eax, %ebx          # Move `main` return value to ebx.
    mov $1, %eax            # Call the `exit` function by using `int 80` (i.e., a system call)
    int $0x80

