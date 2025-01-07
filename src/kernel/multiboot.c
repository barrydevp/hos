#include <kernel/types.h>
#include <kernel/multiboot.h>
#include <kernel/printf.h>

struct multiboot_info mboot;

void multiboot_init(uint32_t magic, uint32_t addr) {
  struct multiboot_tag *tag;

  if (magic != MULTIBOOT2_BOOTLOADER_MAGIC) {
    // printf("Invalid magic number: 0x%x\n", (unsigned)magic);
    return;
  }

  if (addr & 7) {
    // printf("Unaligned mbi: 0x%x\n", addr);
    return;
  }

  for (tag = (struct multiboot_tag *)(addr + 8);
       tag->type != MULTIBOOT_TAG_TYPE_END;
       tag = (struct multiboot_tag *)((multiboot_uint8_t *)tag +
                                      ((tag->size + 7) & ~7))) {
    // printf("Tag 0x%x, Size 0x%x\n", tag->type, tag->size);
    switch (tag->type) {
      // case MULTIBOOT_TAG_TYPE_CMDLINE:
      //   printf("Command line = %s\n",
      //          ((struct multiboot_tag_string *)tag)->string);
      //   break;
      // case MULTIBOOT_TAG_TYPE_BOOT_LOADER_NAME:
      //   printf("Boot loader name = %s\n",
      //          ((struct multiboot_tag_string *)tag)->string);
      //   break;
      // case MULTIBOOT_TAG_TYPE_MODULE:
      //   printf("Module at 0x%x-0x%x. Command line %s\n",
      //          ((struct multiboot_tag_module *)tag)->mod_start,
      //          ((struct multiboot_tag_module *)tag)->mod_end,
      //          ((struct multiboot_tag_module *)tag)->cmdline);
      //   break;
      case MULTIBOOT_TAG_TYPE_BASIC_MEMINFO:
        // dprintf("mem_lower = %uKB, mem_upper = %uKB\n",
        //         ((struct multiboot_tag_basic_meminfo *)tag)->mem_lower,
        //         ((struct multiboot_tag_basic_meminfo *)tag)->mem_upper);
        mboot.multiboot_meminfo = (struct multiboot_tag_basic_meminfo *)tag;
        break;
      // case MULTIBOOT_TAG_TYPE_BOOTDEV:
      //   printf("Boot device 0x%x,%u,%u\n",
      //          ((struct multiboot_tag_bootdev *)tag)->biosdev,
      //          ((struct multiboot_tag_bootdev *)tag)->slice,
      //          ((struct multiboot_tag_bootdev *)tag)->part);
      //   break;
      case MULTIBOOT_TAG_TYPE_MMAP: {
        multiboot_memory_map_t *mmap;

        dprintf("mmap: %u\n", ((struct multiboot_tag_mmap *)tag)->size);

        for (mmap = ((struct multiboot_tag_mmap *)tag)->entries;
             (multiboot_uint8_t *)mmap < (multiboot_uint8_t *)tag + tag->size;
             mmap = (multiboot_memory_map_t
                       *)((unsigned long)mmap +
                          ((struct multiboot_tag_mmap *)tag)->entry_size))
          dprintf(" base_addr = 0x%x%x,"
                  " length = 0x%x%x, type = 0x%x\n",
                  (unsigned)(mmap->addr >> 32),
                  (unsigned)(mmap->addr & 0xffffffff),
                  (unsigned)(mmap->len >> 32),
                  (unsigned)(mmap->len & 0xffffffff), (unsigned)mmap->type);

        mboot.multiboot_mmap = (struct multiboot_tag_mmap *)tag;
        dprintf("mmap: %u\n", mboot.multiboot_mmap->size);
      } break;
      case MULTIBOOT_TAG_TYPE_FRAMEBUFFER: {
        dprintf("framebuffer\n");

        mboot.multiboot_framebuffer = (struct multiboot_tag_framebuffer *)tag;
        // multiboot_uint32_t color;
        // unsigned i;
        // struct multiboot_tag_framebuffer *tagfb =
        //   (struct multiboot_tag_framebuffer *)tag;
        // void *fb = (void *)(unsigned long)tagfb->common.framebuffer_addr;
        //
        // // printf(" type=%u\n", tagfb->common.framebuffer_type);
        // switch (tagfb->common.framebuffer_type) {
        //   case MULTIBOOT_FRAMEBUFFER_TYPE_INDEXED: {
        //     unsigned best_distance, distance;
        //     struct multiboot_color *palette;
        //
        //     palette = tagfb->framebuffer_palette;
        //
        //     color = 0;
        //     best_distance = 4 * 256 * 256;
        //
        //     for (i = 0; i < tagfb->framebuffer_palette_num_colors; i++) {
        //       distance = (0xff - palette[i].blue) * (0xff - palette[i].blue) +
        //                  palette[i].red * palette[i].red +
        //                  palette[i].green * palette[i].green;
        //       if (distance < best_distance) {
        //         color = i;
        //         best_distance = distance;
        //       }
        //     }
        //   } break;
        //
        //   case MULTIBOOT_FRAMEBUFFER_TYPE_RGB:
        //     color = ((1 << tagfb->framebuffer_blue_mask_size) - 1)
        //             << tagfb->framebuffer_blue_field_position;
        //     break;
        //
        //   case MULTIBOOT_FRAMEBUFFER_TYPE_EGA_TEXT:
        //     color = '\\' | 0x0100;
        //     break;
        //
        //   default:
        //     color = 0xffffffff;
        //     break;
        // }
        //
        // for (i = 0; i < tagfb->common.framebuffer_width &&
        //             i < tagfb->common.framebuffer_height;
        //      i++) {
        //   switch (tagfb->common.framebuffer_bpp) {
        //     case 8: {
        //       multiboot_uint8_t *pixel =
        //         fb + tagfb->common.framebuffer_pitch * i + i;
        //       *pixel = color;
        //     } break;
        //     case 15:
        //     case 16: {
        //       multiboot_uint16_t *pixel =
        //         fb + tagfb->common.framebuffer_pitch * i + 2 * i;
        //       *pixel = color;
        //     } break;
        //     case 24: {
        //       multiboot_uint32_t *pixel =
        //         fb + tagfb->common.framebuffer_pitch * i + 3 * i;
        //       *pixel = (color & 0xffffff) | (*pixel & 0xff000000);
        //     } break;
        //
        //     case 32: {
        //       multiboot_uint32_t *pixel =
        //         fb + tagfb->common.framebuffer_pitch * i + 4 * i;
        //       *pixel = color;
        //     } break;
        //   }
        // }
        break;
      }
    }
  }
  tag =
    (struct multiboot_tag *)((multiboot_uint8_t *)tag + ((tag->size + 7) & ~7));
  mboot.multiboot_tag = tag;
}

/* Get highest valid address of memory, usually it's memory size */
uintptr_t get_highest_valid_address(struct multiboot_tag_basic_meminfo *meminfo,
                                    struct multiboot_tag_mmap *tag_mmap) {
  uintptr_t highest_address = 0;
  multiboot_memory_map_t *mmap = tag_mmap->entries;

  while ((multiboot_uint8_t *)mmap <
         (multiboot_uint8_t *)tag_mmap + tag_mmap->size) {
    if (mmap->type == 1 && mmap->len &&
        (uintptr_t)(mmap->addr + mmap->len) - 1 > highest_address) {
      highest_address = (uintptr_t)(mmap->addr + mmap->len) - 1;
    }

    mmap =
      (multiboot_memory_map_t *)((unsigned long)mmap + tag_mmap->entry_size);
  }

  if ((meminfo->mem_upper * 1024) > highest_address) {
    return meminfo->mem_upper * 1024;
  }

  return highest_address;
}

void load_memory_info(uint32_t *highest_address, uint32_t *addressable_size) {
  struct multiboot_tag_mmap *tag_mmap = mboot.multiboot_mmap;
  struct multiboot_tag_basic_meminfo *meminfo = mboot.multiboot_meminfo;
  multiboot_memory_map_t *mmap = tag_mmap->entries;

  // init
  multiboot_uint64_t addr = 0;
  multiboot_uint64_t size = 0;

  while ((multiboot_uint8_t *)mmap <
         (multiboot_uint8_t *)tag_mmap + tag_mmap->size) {
    if (mmap->type == 1 && mmap->len) {
      size += mmap->len;
      if ((mmap->addr + mmap->len - 1) > addr) {
        addr = mmap->addr + mmap->len - 1;
      }
    }

    mmap =
      (multiboot_memory_map_t *)((unsigned long)mmap + tag_mmap->entry_size);
  }

  // if ((uint32_t)(meminfo->mem_upper * 1024) > highest_address) {
  //   highest_address = meminfo->mem_upper * 1024;
  // }

  *highest_address = (uint32_t)(addr & 0xffffffff);
  *addressable_size = (uint32_t)(size & 0xffffffff);
}
