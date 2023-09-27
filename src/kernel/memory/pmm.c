#include <kernel/boot.h>
#include <kernel/memory/pmm.h>
#include <kernel/memory/vmm.h>
#include <kernel/arch.h>
#include <kernel/string.h>

#include <kernel/printf.h>

static uint32_t memsize                 = 0;
static volatile uint32_t *frames_bitmap = NULL;
static uint32_t max_frames              = 0;
static uint32_t used_frames             = 0;
static uint32_t frames_bitmap_size      = 0;
static uint32_t lowest_available        = 0;

/**
 * @brief Mark a physical page frame as in use.
 *
 * Sets the bitmap allocator bit for a frame.
 *
 * @param frame is the frame number(index) (not Address of the frame!)
 */
void pmm_frame_set(uint32_t frame) {
  uint32_t index  = FRAME_INDEX(frame);
  uint32_t offset = FRAME_OFFSET(frame);
  frames_bitmap[index] |= ((uint32_t)1 << offset);
  asm("" ::: "memory");
}

/**
 * @brief Mark a physical page frame as in use.
 *
 * Sets the bitmap allocator bit for a frame.
 *
 * @param frame_addr Address of the frame (not index!)
 */
void pmm_frame_seta(uintptr_t frame_addr) {
  /* If the frame is within bounds... */
  // if (frame_addr < max_frames * FRAME_SIZE) {
  uint32_t frame  = frame_addr >> FRAME_SHIFT;
  uint32_t index  = FRAME_INDEX(frame);
  uint32_t offset = FRAME_OFFSET(frame);
  frames_bitmap[index] |= ((uint32_t)1 << offset);
  asm("" ::: "memory");
  // }
}

/**
 * @brief Mark a physical page frame as available.
 *
 * Clears the bitmap allocator bit for a frame.
 *
 * @param frame is the frame number(index) (not Address of the frame!)
 */
void pmm_frame_unset(uint32_t frame) {
  uint32_t index  = FRAME_INDEX(frame);
  uint32_t offset = FRAME_OFFSET(frame);
  frames_bitmap[index] &= ~((uint32_t)1 << offset);
  asm("" ::: "memory");
  if (frame < lowest_available) {
    lowest_available = frame;
  }
}

/**
 * @brief Mark a physical page frame as available.
 *
 * Clears the bitmap allocator bit for a frame.
 *
 * @param frame_addr Address of the frame (not index!)
 */
void pmm_frame_unseta(uintptr_t frame_addr) {
  /* If the frame is within bounds... */
  // if (frame_addr < max_frames * FRAME_SIZE) {
  uint32_t frame  = frame_addr >> FRAME_SHIFT;
  uint32_t index  = FRAME_INDEX(frame);
  uint32_t offset = FRAME_OFFSET(frame);
  frames_bitmap[index] &= ~((uint32_t)1 << offset);
  asm("" ::: "memory");
  if (frame < lowest_available) {
    lowest_available = frame;
  }
  // }
}

/**
 * @brief Determine if a physical page is available for use.
 *
 * @param frame is the frame number(index) (not Address of the frame!)
 * @returns 0 if available, 1 otherwise.
 */
bool pmm_frame_test(uint32_t frame) {
  uint32_t index  = FRAME_INDEX(frame);
  uint32_t offset = FRAME_OFFSET(frame);
  asm("" ::: "memory");
  return !!(frames_bitmap[index] & ((uint32_t)1 << offset));
}

/**
 * @brief Find the first available frame from the bitmap.
 */
uint32_t pmm_first_free_frame() {
  uint32_t i, j;
  for (i = FRAME_INDEX(lowest_available); i < FRAME_INDEX(max_frames); ++i) {
    // all bits are set (uint32_t)-1 = 0xFFFFFFFF
    if (frames_bitmap[i] != (uint32_t)-1)
      // uinit32_t has 4 bytes, each bytes has 8 bits => total = 4*8 bits
      for (j = 0; j < 4 * 8; ++j) {
        uint32_t test_frame = 1 << j;
        if (!(frames_bitmap[i] & test_frame)) {
          int32_t out = (i << 5) + j;
          // FIXME: You may want to uncomment following line for searching optimization.
          /* 
           * Look at the first loop in this function with iteration start from this
           * lowest_available. But if we use this optimization, that may cause fragmentation
           * and affect the allocation of n contiguous free frames.
           */
          // lowest_available = out;
          return out;
        }
      }
  }

  return 0;
}

/**
 * @brief Find the first range of @p n contiguous free frames.
 *
 * If a large enough region could not be found, results are fatal.
 */
uint32_t pmm_first_nfree_frames(size_t n) {
  if (n == 0)
    return 0;

  if (n == 1)
    return pmm_first_free_frame();

  // TODO: compare perfomance of 3 following methods
  // Method 1: mos
  // for (uint32_t i = 0; i < FRAME_INDEX(max_frames); i++) {
  //   if (frames[i] != 0xffffffff)
  //     for (uint32_t j = 0; j < 32; j++) { // test each frame (bit) if available
  //       uint32_t bit = 1 << j;
  //       if (!(frames[i] & bit)) { // if available => check next n-1 frame
  //         int startingBit = i * 32;
  //         startingBit += j; // get the free bit in the dword at index i
  //
  //         uint32_t used = 0; // loop through each bit to see if its enough space
  //         for (uint32_t count = 0; count <= n; count++) {
  //           if (pmm_frame_test(startingBit + count))
  //             used++; // this bit is clear (free frame)
  //         }
  //         if (!used)
  //           return i * 32 + j; // free count==size needed; return index
  //       }
  //     }
  // }

  // Method 2: toaruos
  // for (uint32_t i = 0; i < max_frames; ++i) {
  //   uint32_t bad = 0;
  //   for (uint32_t j = 0; j < n; ++j) {
  //     if (pmm_frame_test(i + j)) {
  //       bad = j + 1;
  //     }
  //   }
  //   if (!bad) {
  //     return (int32_t)i;
  //   }
  // }

  // Method 3: barry (me)
  uint32_t i   = 0;
  size_t avail = 0;
  while (i < max_frames) {
    if (frames_bitmap[FRAME_INDEX(i)] != (uint32_t)-1) {
      if (pmm_frame_test(i++)) {
        avail = 0;
        continue;
      }
      avail++;

      if (avail == n) {
        return (int32_t)(i - n);
      }
    } else {
      i += 32;
      avail = 0;
    }
  }

  return 0;
}

static inline void pmm_mark_frame_used(uint32_t frame) {
  pmm_frame_set(frame);
  used_frames++;
}

static inline uint32_t __pmm_allocate_frame(void) {
  // if (max_frames <= used_frames)
  //   return 0;

  uint32_t frame = pmm_first_free_frame();
  if (frame == 0) {
    dprintf("Out of memory.\n");
    arch_fatal();
    return 0;
  }

  pmm_mark_frame_used(frame);

  return frame;
}

uint32_t pmm_allocate_frame(void) {
  return __pmm_allocate_frame();
}

uintptr_t pmm_allocate_frame_addr(void) {
  uint32_t frame = __pmm_allocate_frame();

  uintptr_t addr = frame << FRAME_SHIFT;
  return addr;
}

static inline uint32_t __pmm_allocate_frames(size_t n) {
  // if (max_frames - used_frames < n)
  //   return 0;

  uint32_t start_frame = pmm_first_nfree_frames(n);
  if (start_frame == 0) {
    dprintf("Out of memory.\n");
    arch_fatal();
    return 0;
  }

  for (uint32_t i = 0; i < n; ++i) { pmm_mark_frame_used(start_frame + i); }

  return start_frame;
}

uint32_t pmm_allocate_frames(size_t n) {
  return __pmm_allocate_frames(n);
}

uintptr_t pmm_allocate_frames_addr(size_t n) {
  uint32_t start_frame = __pmm_allocate_frames(n);

  uintptr_t addr = start_frame << FRAME_SHIFT;
  return addr;
}

void pmm_free_frame(uintptr_t frame_addr) {
  // uint32_t addr = (uint32_t)p;
  // uint32_t frame = addr / PMM_FRAME_SIZE;
  //
  // pmm_frame_unset(frame);

  used_frames--;
  pmm_frame_unseta(frame_addr);
}

void pmm_init_test(uint32_t *frames_list, uint32_t size) {
  frames_bitmap = frames_list;
  max_frames    = size * 32;
}

void pmm_init_region(uintptr_t addr, uint32_t length) {
  uint32_t frame_start = addr / FRAME_SIZE;
  uint32_t frames      = (length & FRAME_MASK) >> FRAME_SHIFT;

  for (uint32_t i = 0; i < frames; ++i) { pmm_frame_unset(i + frame_start); }
}

void pmm_deinit_region(uintptr_t addr, uint32_t length) {
  uint32_t frame_start = addr / FRAME_SIZE;
  uint32_t frames      = (length & FRAME_MASK) >> FRAME_SHIFT;

  for (uint32_t i = 0; i < frames; ++i) { pmm_frame_set(i + frame_start); }
}

// void pmm_frame_seta(uint32_t paddr) {
//   uint32_t frame = paddr / PMM_FRAME_SIZE;
//   if (!pmm_frame_test(frame)) {
//     pmm_frame_set(frame);
//     used_frames++;
//   }
// }

uint32_t get_total_frames() {
  return max_frames;
}

void pmm_init(boot_info_t *boot_info) {
  struct multiboot_info *mboot = boot_info->multiboot_header;
  // memsize = (meminfo->mem_lower + meminfo->mem_upper) * 1024;
  struct multiboot_tag_mmap *tag_mmap = mboot->multiboot_mmap;
  memsize                             = boot_info->highest_address;

  // FIXME: We have an unhandled problem here, the multiboot is allocated
  // from kernel_phy_end, so if we use kernel_phy_end as a start address
  // for our data, it will overlapped with the multiboot data and will cause
  // error. => For this situation we have a quick fix by page align the start
  // address and hope it will not overlapped with the multiboot, in the
  // future, a stable fix may need to take the multiboot size into account for
  // kernel_phy_end.
  uint32_t addressable_phy = PAGE_ALIGN(boot_info->kernel_phy_end);
  uint32_t addressable     = PAGE_ALIGN(boot_info->kernel_end);
  // dprintf("mmap: %p, end: %p, addr: %p\n", tag_mmap, boot_info->kernel_phy_end,
  //         addressable_phy);

  /* Setup page allocator frames bitmap */
  max_frames = (memsize >> FRAME_SHIFT) + 1;
  frames_bitmap_size =
    (FRAME_INDEX(max_frames - 1) + 1) * sizeof(*frames_bitmap);

  // // we want frames_bitmap frame(page) aligned, bitmap fully fit into n frames(pages),
  // // in another words, n frames(pages) contain only bitmap data
  // frames_bitmap_size = FRAME_ALIGN(frames_bitmap_size);

  /* Allocate bitmap from KERNEL_END (KERNEL_HEAP_STRART?) */
  // ALERT!: As we are already in paging mode and pmm_init run before vmm_init
  // so in order to run next following line, frame bitmap setup,
  // the physical memory of bitmap must be mapped in PDT.
  // Remember we first map 4MB of kernel (1MB identity + 3MB kernel) in bootstrap.S
  // and the size of bitmap for 4GB memory is 128KB so if 3MB is not enough
  // for those we must increase them in bootstrap.S or (maybe use another approach?
  // that setup an temporary paging after boot)
  frames_bitmap = (uint32_t *)(addressable);
  /* Mark all frames as used */
  memset((void *)frames_bitmap, 0xFF, frames_bitmap_size);
  addressable_phy += frames_bitmap_size;
  addressable += frames_bitmap_size;

  dprintf("mmap: %u\n", tag_mmap->size);
  /* Map valid memory into bitmap - memory region initialization */
  multiboot_memory_map_t *mmap = tag_mmap->entries;

  while ((multiboot_uint8_t *)mmap <
         (multiboot_uint8_t *)tag_mmap + tag_mmap->size) {
    if (mmap->type == 1) {
      pmm_init_region(mmap->addr, mmap->len);
    }

    mmap =
      (multiboot_memory_map_t *)((unsigned long)mmap + tag_mmap->entry_size);
  }

  // frame 0 (first frame, start from addr 0x0) is always used because
  // we use NULL = 0x0 as a null_pointer
  pmm_frame_set(0);

  /* Mark some initial region as used */
  // boot region
  pmm_deinit_region(0x0, boot_info->bootloader_phy_end);
  // running-kernel region
  pmm_deinit_region(boot_info->kernel_phy_start, boot_info->kernel_size);
  /* Now mark everything up to addressable_phy as in use */
  pmm_deinit_region(boot_info->kernel_phy_end,
                    addressable_phy - boot_info->kernel_phy_end);
  // video region
  pmm_deinit_region(boot_info->video_phy_start,
                    boot_info->video_phy_end - boot_info->video_phy_start);

  /* Count available and used frames */
  for (uint32_t i = 0; i < FRAME_INDEX(max_frames); ++i) {
    for (uint32_t j = 0; j < 4 * 8; ++j) {
      uint32_t test_frame = 1 << j;
      if ((frames_bitmap[i] & test_frame)) {
        used_frames++;
      }
    }
  }
  dprintf("PMM summary:\n"
          " frames: max=%u used=%u free=%u\n"
          " bitmap: virt=0x%p size=%u\n",
          max_frames, used_frames, max_frames - used_frames, frames_bitmap,
          frames_bitmap_size);

  // log("PMM: Done");
}
