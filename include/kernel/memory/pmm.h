#pragma once

#include <kernel/types.h>
#include <kernel/kernel.h>
#include <kernel/boot.h>

#define PMM_FRAMES_PER_BYTE 8
#define PMM_FRAME_SIZE 4096
#define PMM_FRAME_ALIGN PMM_FRAME_SIZE
#define FRAMES_PER_BYTE 8
#define FRAME_SIZE 4096
#define FRAME_SHIFT 12
#define FRAME_MASK (~(FRAME_SIZE - 1))
#define FRAME_LOW_MASK (FRAME_SIZE - 1)
#define FRAME_ALIGN(addr) (((addr) + FRAME_LOW_MASK) & FRAME_MASK)

#define FRAME_INDEX(b) ((b) >> 5)
#define FRAME_OFFSET(b) ((b)&0x1F)

void pmm_frame_set(uint32_t frame);
void pmm_frame_seta(uintptr_t frame_addr);
void pmm_frame_unset(uint32_t frame);
void pmm_frame_unseta(uintptr_t frame_addr);
bool pmm_frame_test(uint32_t frame);
int32_t pmm_first_free_frame();
int32_t pmm_first_nfree_frames(size_t n);
void pmm_regions(struct multiboot_tag_mmap *multiboot_mmap);
void pmm_init_region(uintptr_t addr, uint32_t length);
void pmm_deinit_region(uintptr_t add, uint32_t length);
uint32_t pmm_allocate_frame(void);
uintptr_t pmm_allocate_frame_addr(void);
uint32_t pmm_allocate_frames(size_t n);
uintptr_t pmm_allocate_frames_addr(size_t n);
void pmm_free_frame(uintptr_t frame_addr);
uint32_t get_total_frames();

void pmm_init(struct boot_info_t *boot_info);

void pmm_init_test(uint32_t *frames_list, uint32_t size);
