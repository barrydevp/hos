#pragma once

/** Kernel infomation */
extern void *_bootloader_start;
extern void *_bootloader_end;
extern void *_kernel_higher_half;
extern void *_kernel_start;
extern void *_kernel_text_start;
extern void *_kernel_text_end;
extern void *_kernel_data_start;
extern void *_kernel_data_end;
extern void *_kernel_stack_top;
extern void *_kernel_end;

#define BOOTLOADER_START (uint32_t)(&_bootloader_start)
#define BOOTLOADER_END (uint32_t)(&_bootloader_end)

#define KERNEL_HIGHER_HALF (uint32_t)(&_kernel_higher_half)
#define KERNEL_START (uint32_t)(&_kernel_start)
#define KERNEL_TEXT_START (uint32_t)(&_kernel_text_start)
#define KERNEL_TEXT_END (uint32_t)(&_kernel_text_end)
#define KERNEL_DATA_START (uint32_t)(&_kernel_data_start)
#define KERNEL_DATA_END (uint32_t)(&_kernel_data_end)
#define KERNEL_STACK_TOP (uint32_t)(&_kernel_stack_top)
#define KERNEL_END (uint32_t)(&_kernel_end)

#define KERNEL_LOW_SIZE 896 // MB
#define KERNEL_STACK_SIZE 0x100000 // 1MB
#define KERNEL_HEAP_END 0xF0000000
#define KERNEL_HEAP_START 0xD0000000
#define USER_HEAP_START 0x00000000
#define USER_HEAP_END 0x40000000
#define FRAMEBUFFER_START KERNEL_HEAP_END

#define KERNEL_PDE_START 768 // virtAddress from 0xC0000000
#define KERNEL_INIT_NPDE 1 // we use one PDE mean one page table (4MB)

/** utilities */
#define __ALIGN_UP(addr, aligned) (((addr) + (aligned - 1)) & (~(aligned - 1)))
#define __ALIGN_DOWN(addr, aligned) ((addr) & (~(aligned - 1)))

#define KB (1024u)
#define MB (1024u * 1024u)
#define GB (1024u * 1024u * 1024u)

/** method */

int kmain(void);
