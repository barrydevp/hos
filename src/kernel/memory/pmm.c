#include <kernel/memory/pmm.h>
#include <kernel/multiboot.h>
#include <kernel/string.h>

static uint32_t memsize = 0;
static volatile uint32_t *frames_bitmap = NULL;
static uint32_t max_frames = 0;
static uint32_t used_frames = 0;
static uint32_t frames_bitmap_size = 0;
static uint32_t lowest_available = 0;
static char *heapStart = NULL;

/**
 * @brief Mark a physical page frame as in use.
 *
 * Sets the bitmap allocator bit for a frame.
 *
 * @param frame is the frame number(index) (not Address of the frame!)
 */
void pmm_frame_set(uint32_t frame) {
  uint32_t index = FRAME_INDEX(frame);
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
  uint32_t frame = frame_addr >> FRAME_SHIFT;
  uint32_t index = FRAME_INDEX(frame);
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
  uint32_t index = FRAME_INDEX(frame);
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
  uint32_t frame = frame_addr >> FRAME_SHIFT;
  uint32_t index = FRAME_INDEX(frame);
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
  uint32_t index = FRAME_INDEX(frame);
  uint32_t offset = FRAME_OFFSET(frame);
  asm("" ::: "memory");
  return !!(frames_bitmap[index] & ((uint32_t)1 << offset));
}

/**
 * @brief Find the first available frame from the bitmap.
 */
int32_t pmm_first_free_frame() {
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

  dprintf("Out of memory.\n");

  return -1;
}

/**
 * @brief Find the first range of @p n contiguous free frames.
 *
 * If a large enough region could not be found, results are fatal.
 */
int32_t pmm_first_nfree_frames(size_t n) {
  if (n == 0)
    return -1;

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
  uint32_t i = 0;
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

  return -1;
}

inline void pmm_mark_frame_used(uint32_t frame) {
  pmm_frame_set(frame);
  used_frames++;
}

uintptr_t pmm_allocate_frame(void) {
  // if (max_frames <= used_frames)
  //   return 0;

  int frame = pmm_first_free_frame();
  if (frame == -1)
    return 0;

  pmm_mark_frame_used(frame);

  uintptr_t addr = frame << FRAME_SHIFT;
  return addr;
}

uintptr_t pmm_allocate_frames(size_t n) {
  // if (max_frames - used_frames < n)
  //   return 0;

  int frame = pmm_first_nfree_frames(n);
  if (frame == -1)
    return 0;

  for (uint32_t i = 0; i < n; ++i) {
    pmm_mark_frame_used(frame + i);
  }

  uint32_t addr = frame << FRAME_SHIFT;
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
  max_frames = size * 32;
}

void pmm_init(struct multiboot_tag_basic_meminfo *meminfo,
              struct multiboot_tag_mmap *tag_mmap) {
  // memsize = (meminfo->mem_lower + meminfo->mem_upper) * 1024;

  memsize = get_highest_valid_address(meminfo, tag_mmap);

  /* Setup page allocator frames bitmap */
  used_frames = max_frames = memsize >> FRAME_SHIFT;
  frames_bitmap_size = FRAME_INDEX(max_frames) * sizeof(*frames_bitmap);
  // we want frames_bitmap frame(page) aligned, bitmap fully fit into n frames(pages),
  // in another words, n frames(pages) contain only bitmap data
  frames_bitmap_size = FRAME_ALIGN(frames_bitmap_size);
  /* Allocate bitmap from KERNEL_END (KERNEL_HEAP_STRART?) */
  frames_bitmap = (uint32_t *)FRAME_ALIGN(KERNEL_END);
  /* Mark all frames as used */
  memset((void *)frames_bitmap, 0xFF, frames_bitmap_size);

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

  /* Mark some kernel region as used */
  // // boot region
  // pmm_deinit_region(0x0, KERNEL_BOOT);
  // // running-kernel region
  // pmm_deinit_region((uintptr_t)(KERNEL_START - KERNEL_HIGHER_HALF),
  //                   KERNEL_END - KERNEL_START);
  // // frames_bitmap region
  // pmm_deinit_region((uintptr_t)(KERNEL_END - KERNEL_HIGHER_HALF),
  //                   (uint32_t)frames_bitmap - KERNEL_END + frames_bitmap_size);

  /* Now mark everything up to end of bitmap as in use */
  pmm_deinit_region(0x0, (uint32_t)frames_bitmap + frames_bitmap_size);

  /* Count available and used frames */
  for (uint32_t i = 0; i < FRAME_INDEX(max_frames); ++i) {
    for (uint32_t j = 0; j < 4 * 8; ++j) {
      uint32_t test_frame = 1 << j;
      if ((frames_bitmap[i] & test_frame)) {
        used_frames++;
      }
    }
  }

  /* Set the start address of HEAP region */
  heapStart = (char *)frames_bitmap + frames_bitmap_size;

  // log("PMM: Done");
}

void pmm_regions(struct multiboot_tag_mmap *multiboot_mmap) {
  for (struct multiboot_mmap_entry *mmap = multiboot_mmap->entries;
       (multiboot_uint8_t *)mmap <
       (multiboot_uint8_t *)multiboot_mmap + multiboot_mmap->size;
       mmap = (struct multiboot_mmap_entry *)((uint32_t)mmap +
                                              multiboot_mmap->entry_size)) {
    if (mmap->type > 4 && mmap->addr == 0)
      break;

    if (mmap->type == 1)
      pmm_init_region(mmap->addr, mmap->len);
  }
}

void pmm_init_region(uintptr_t addr, uint32_t length) {
  uint32_t frames = (addr + (length & FRAME_MASK)) >> FRAME_SHIFT;

  for (uint32_t i = 0; i < frames; ++i) {
    pmm_frame_unset(i);
  }
}

void pmm_deinit_region(uintptr_t addr, uint32_t length) {
  uint32_t frames = (addr + (length & FRAME_MASK)) >> FRAME_SHIFT;

  for (uint32_t i = 0; i < frames; ++i) {
    pmm_frame_set(i);
  }
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
