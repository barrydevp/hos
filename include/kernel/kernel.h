#pragma once

/** Kernel infomation */
extern void *_kernel_boot;
extern void *_kernel_higher_half;
extern void *_kernel_start;
extern void *_kernel_text_start;
extern void *_kernel_text_end;
extern void *_kernel_data_start;
extern void *_kernel_data_end;
extern void *_kernel_end;

#define KERNEL_BOOT (uint32_t)(&_kernel_boot)
#define KERNEL_HIGHER_HALF (uint32_t)(&_kernel_higher_half)
#define KERNEL_START (uint32_t)(&_kernel_start)
#define KERNEL_TEXT_START (uint32_t)(&_kernel_text_start)
#define KERNEL_TEXT_END (uint32_t)(&_kernel_text_end)
#define KERNEL_DATA_START (uint32_t)(&_kernel_data_start)
#define KERNEL_DATA_END (uint32_t)(&_kernel_data_end)
#define KERNEL_END (uint32_t)(&_kernel_end)

/** method */

int kmain(void);
