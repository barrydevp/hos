/* This is user defined multiboot file not containing any multiboot specification
 * Related multiboot specification in multiboot2.h
 */
#pragma once

#include <kernel/types.h>
#include <kernel/multiboot2.h>

struct multiboot_info {
  struct multiboot_tag *multiboot_tag;

  struct multiboot_tag_basic_meminfo *multiboot_meminfo;
  struct multiboot_tag_mmap *multiboot_mmap;
  struct multiboot_tag_framebuffer *multiboot_framebuffer;
};

extern struct multiboot_info mboot;

void multiboot_init(uint32_t addr, uint32_t magic);
uintptr_t get_highest_valid_address(struct multiboot_tag_basic_meminfo *meminfo,
                                    struct multiboot_tag_mmap *tag_mmap);
