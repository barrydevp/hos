#include <kernel/memory/mmu.h>
#include <kernel/memory/vmm.h>
#include <kernel/assert.h>
#include <kernel/string.h>
#include <kernel/printf.h>

/// Overhead given by the block_t itself.
#define OVERHEAD sizeof(block_t)
/// User heap initial size ( 1 Megabyte).
#define UHEAP_INITIAL_SIZE (1 * 1024 * 1024)

/// @brief Identifies a block of memory.
typedef struct block_t {
  ///  scheme:
  ///  - first 31 bit: contains real size
  ///  - last 1 bit: determine the block state (free or not)
  ///  To calculate the real size, set to zero the last bit.
  ///  We use this scheme to make the block_t naturally aligned and small.
  ///  In this case the size of heap block in 32bit system will never exceed
  ///  31 bits, so this scheme is feasible
  uint32_t size;
  /// Pointer to the next free block.
  struct block_t *nextfree;
  /// Pointer to the next block.
  struct block_t *next;
} block_t;

/// Kernel heap area.
static vm_area_struct_t kernel_heap;
/// Top of the kernel heap.
static uint32_t kernel_heap_top;

/// @brief Given the field size in a Block(which contain free/alloc
///        information), extract the size.
/// @param size
/// @return
static inline uint32_t block_get_real_size(uint32_t size) {
  return (size >> 1U) << 1U;
}

/// @brief Sets the free/alloc bit of the size field.
static inline void block_set_free(uint32_t *size, int x) {
  (*size) = (x) ? ((*size) | 1U) : ((*size) & 0xFFFFFFFE);
}

/// @brief Checks if a block is freed or allocated.
static inline bool block_is_free(block_t *block) {
  if (block == NULL)
    return 0;

  return (block->size & 1U);
}

/// @brief       Checks if the given size fits inside the block.
/// @param block The given block.
/// @param size  The size to check
/// @return
static inline bool block_is_fit(block_t *block, uint32_t size) {
  assert(block && "Received null block.");

  return (block->size >= block_get_real_size(size)) && block_is_free(block);
}

/// @brief Removes the block from freelist.
static inline void block_remove_from_freelist(block_t *block,
                                              uint32_t *freelist) {
  assert(block && "Received null block.");
  assert(freelist && "Freelist is a null pointer.");

  block_t *first_free_block = (block_t *)*freelist;
  assert(first_free_block && "Freelist is empty.");

  if (block == first_free_block) {
    *freelist = (uint32_t)block->nextfree;
  } else {
    block_t *prev = first_free_block;
    while (prev != NULL && prev->nextfree != block)
      prev = prev->nextfree;

    if (prev) {
      prev->nextfree = block->nextfree;
    }
  }

  block->nextfree = NULL;
}

/// @brief Add the block to the free list.
static inline void block_add_to_freelist(block_t *block, uint32_t *freelist) {
  assert(block && "Received null block.");
  assert(freelist && "Freelist is a null pointer.");
  block_t *first_free_block = (block_t *)*freelist;
  block->nextfree = first_free_block;
  *freelist = (uint32_t)block;
}

/// @brief Find the best fitting block in the memory pool.
static inline block_t *block_find_best_fitting(uint32_t size,
                                               uint32_t *freelist) {
  assert(freelist && "Freelist is a null pointer.");
  block_t *first_free_block = (block_t *)*freelist;

  if (first_free_block == NULL) {
    return NULL;
  }
  block_t *best_fitting = NULL;
  for (block_t *current = first_free_block; current;
       current = current->nextfree) {
    if (!block_is_fit(current, size)) {
      continue;
    }
    if ((best_fitting == NULL) || (current->size < best_fitting->size)) {
      best_fitting = current;
    }
  }
  return best_fitting;
}

/// @brief Given a block, finds its previous block.
static inline block_t *block_get_previous_block(block_t *block,
                                                uint32_t *head) {
  assert(block && "Received null block.");
  assert(head && "The head of the list is not set.");

  block_t *head_block = (block_t *)*head;
  assert(head_block && "The head of the list is not set.");

  if (block == head_block) {
    return NULL;
  }
  block_t *prev = head_block;

  // FIXME: Sometimes enters infinite loop!
  while (prev->next != block) {
    prev = prev->next;
  }

  return prev;
}

/// @brief Given a block, finds its next block.
static inline block_t *block_get_next_block(block_t *block, uint32_t *tail) {
  assert(block && "Received null block.");
  assert(tail && "The tail of the list is not set.");

  block_t *tail_block = (block_t *)*tail;
  assert(tail_block && "The head of the list is not set.");

  if (block == tail_block) {
    return NULL;
  }

  return block->next;
}

/// @brief Find the current user heap.
/// @return The heap structure if heap exists, otherwise NULL.
static vm_area_struct_t *__find_user_heap() {
  // Get the memory descriptor of the current process.
  // task_struct *current_task = scheduler_get_current_process();
  // if (current_task == NULL) {
  //   pr_emerg("There is no current task!\n");
  //   return NULL;
  // }
  // mm_struct_t *current_mm = current_task->mm;
  // if (current_mm == NULL) {
  //   pr_emerg("The mm_struct of the current task is not initialized!\n");
  //   return NULL;
  // }
  // // Get the starting address of the heap.
  // uint32_t start_heap = current_mm->start_brk;
  // // If not set return NULL.
  // if (start_heap == 0) {
  //   return NULL;
  // }
  // // Otherwise find the respective heap segment.
  // vm_area_struct_t *segment = NULL;
  // list_for_each_decl(it, &current_mm->mmap_list) {
  //   segment = list_entry(it, vm_area_struct_t, vm_list);
  //   if (segment->vm_start == start_heap) {
  //     return segment;
  //   }
  // }
  return NULL;
}

/// @brief Extends the provided heap of the given increment.
/// @param heap_top  Current top of the heap.
/// @param heap      Pointer to the heap.
/// @param increment Increment to the heap.
/// @return Pointer to the old top of the heap, ready to be used.
static void *__do_brk(uint32_t *heap_top, vm_area_struct_t *heap,
                      int increment) {
  assert(heap_top && "Pointer to the current top of the heap is NULL.");
  assert(heap && "Pointer to the heap is NULL.");
  dprintf("BRK> %s: heap_start: %p, free space: %d\n",
          (heap == &kernel_heap) ? "KERNEL" : "USER",
          (uint32_t *)heap->vm_start, heap->vm_end - (uint32_t)heap_top);
  if (increment > 0) {
    // Compute the new boundary.
    uint32_t size = (uint32_t)increment;
    uint32_t new_boundary = *heap_top + size;
    // If new boundary is smaller or equal to end, simply
    // update the heap_top to the new boundary and return
    // the old heap_top.

    if (new_boundary <= heap->vm_end) {
      // Save the old top of the heap.
      uint32_t old_heap_top = *heap_top;
      // Overwrite the top of the heap.
      *heap_top = new_boundary;
      // Return the old top of the heap.

      for (uint32_t i_virt = 0; i_virt < size; i_virt += PAGE_SIZE) {
        vmm_create_page((uint32_t)heap_top + i_virt, PML_USER_ACCESS);
      }

      return (void *)old_heap_top;
    } else {
      dprintf("Out of heap memory.\n");
      goto ret_null;
    }
  }

ret_null:
  return NULL;
}

/// @brief Allocates size bytes of uninitialized storage.
/// @param heap Heap from which we get the unallocated memory.
/// @param size Size of the desired memory area.
/// @return Pointer to the allocated memory area.
static void *__do_malloc(vm_area_struct_t *heap, size_t size) {
  if (size == 0)
    return NULL;

  // Get:
  // 1) First memory block.
  // block_t *head = NULL;
  // 2) Last memory block.
  // block_t *tail = NULL;
  // 3)  All the memory blocks that are freed.
  // block_t *freelist = NULL;

  // We will use these in writing.
  uint32_t *head = (uint32_t *)(heap->vm_start);
  uint32_t *tail = (uint32_t *)(heap->vm_start + sizeof(block_t *));
  uint32_t *freelist = (uint32_t *)(heap->vm_start + 2 * sizeof(block_t *));
  // assert(head && tail && freelist && "Heap block lists point to null.");

  // We will use these others in reading.
  block_t *head_block = (block_t *)*head;
  block_t *tail_block = (block_t *)*tail;
  // block_t *first_free_block = (block_t *) *freelist;

  // Calculate real size that's used, round it to multiple of 16.
  uint32_t rounded_size = __ALIGN_UP(size, 16);
  // The block size takes into account also the block_t overhead.
  uint32_t block_size = rounded_size + OVERHEAD;

  // Find bestfit in avl tree. This bestfit function will remove the
  // best-fit node when there is more than one such node in tree.
  block_t *best_fitting = block_find_best_fitting(rounded_size, freelist);

  if (best_fitting != NULL) {
    // and! put a SIZE to the last four byte of the chunk
    char *block_ptr = (void *)best_fitting;
    // Store a pointer to the next block.
    void *stored_next_block = block_get_next_block(best_fitting, tail);
    // Get the size of the chunk.
    uint32_t chunk_size = block_get_real_size(best_fitting->size) + OVERHEAD;
    // Get what's left.
    uint32_t remaining_size = chunk_size - block_size;
    // Get the real size.
    uint32_t real_size = (remaining_size < (8 + OVERHEAD)) ? chunk_size :
                                                             block_size;
    // Set the size of the best fitting block.
    best_fitting->size = real_size - OVERHEAD;
    // Set the content of the block as free.
    block_set_free(&(best_fitting->size), 0);
    // Store the base pointer.
    void *base_ptr = block_ptr;

    block_ptr = (char *)block_ptr + real_size;

    if (remaining_size < (8 + OVERHEAD)) {
      goto no_split;
    } else if (remaining_size >= (8 + OVERHEAD)) {
      if (block_is_free(stored_next_block)) {
        // Choice b)  merge!
        // Gather info about next block
        void *nextblock = stored_next_block;
        block_t *n_nextblock = nextblock;
        /* Remove next from list because it no longer exists(just
                 * unlink it)
                 */
        block_remove_from_freelist(n_nextblock, freelist);

        // Merge!
        block_t *t = (block_t *)block_ptr;
        t->size = remaining_size + block_get_real_size(n_nextblock->size);
        block_set_free(&(t->size), 1);

        t->next = block_get_next_block(stored_next_block, tail);

        if (nextblock == tail_block) {
          // I don't want to set it to tail now, instead, reclaim it
          *tail = (uint32_t)t;
          // int reclaimSize = block_get_real_size(t->size) + OVERHEAD;
          // ksbrk(-reclaimSize);
          // goto no_split;
        }
        // then add merged one into the front of the list
        block_add_to_freelist(t, freelist);
      } else {
        // Choice a)  seperate!
        block_t *putThisBack = (block_t *)block_ptr;
        putThisBack->size = remaining_size - OVERHEAD;
        block_set_free(&(putThisBack->size), 1);

        putThisBack->next = stored_next_block;

        if (base_ptr == tail_block) {
          *tail = (uint32_t)putThisBack;
          // int reclaimSize = block_get_real_size(putThisBack->size) +OVERHEAD;
          // ksbrk(-reclaimSize);
          // goto no_split;
        }
        block_add_to_freelist(putThisBack, freelist);
      }

      ((block_t *)base_ptr)->next = (block_t *)block_ptr;
    }
no_split:
    // Remove the block from the free list.
    block_remove_from_freelist(base_ptr, freelist);

    return (char *)base_ptr + sizeof(block_t);
  } else {
    uint32_t realsize = block_size;
    block_t *ret;

    if (heap == &kernel_heap) {
      ret = ksbrk(realsize);
    } else {
      ret = usbrk(realsize);
    }

    assert(ret != NULL && "Heap is running out of space\n");

    if (!head_block) {
      *head = (uint32_t)ret;
    } else {
      tail_block->next = ret;
    }

    ret->next = NULL;
    ret->nextfree = NULL;
    *tail = (uint32_t)ret;

    void *save = ret;

    /* After sbrk(), split the block into half [block_size  | the rest],
         * and put the rest into the tree.
         */
    ret->size = block_size - OVERHEAD;
    block_set_free(&(ret->size), 0);
    // Set the block allocated.
    // ptr = ptr + block_size - sizeof(uint32_t);
    // trailing_space = ptr;
    // *trailing_space = ret->size;

    // Now, return it!
    return (char *)save + sizeof(block_t);
  }
}

/// @brief Allocates size bytes of uninitialized storage with block align.
static void *__do_malloc_align(vm_area_struct_t *heap, uint32_t size) {
  return NULL;
  // if (size == 0)
  //   return NULL;
  //
  // // Get:
  // // 1) First memory block.
  // // static block_t *head = NULL;
  // // 2) Last memory block.
  // // static block_t *tail = NULL;
  // // 3) All the memory blocks that are freed.
  // // static block_t *freelist = NULL;
  //
  // // We will use these in writing.
  // uint32_t *head = (uint32_t *)(heap->vm_start);
  // uint32_t *tail = (uint32_t *)(heap->vm_start + sizeof(block_t *));
  // uint32_t *freelist = (uint32_t *)(heap->vm_start + 2 * sizeof(block_t *));
  // assert(head && tail && freelist && "Heap block lists point to null.");
  //
  // // We will use these others in reading.
  // block_t *head_block = (block_t *)*head;
  // block_t *tail_block = (block_t *)*tail;
  // // block_t *first_free_block = (block_t *) *freelist;
  //
  // // Calculate real size that's used, round it to multiple of 16.
  // uint32_t rounded_size = __ALIGN_UP(size, 16);
  //
  // /* Find bestfit in avl tree. This bestfit function will remove
  //   * thebest-fit node when there is more than one such node in tree.
  //   */
  // block_t *best_fitting = block_find_best_fitting(rounded_size, freelist);
  // if (best_fitting != NULL && (IS_ALIGN(best_fitting + sizeof(block_t)))) {
  //   return kmalloc(size);
  // } else {
  //   void *needed_addr = (void *)ADDR_ALIGN(
  //     ((uint32_t)kernel_heap_top + sizeof(block_t)) & 0xFFFFF000);
  //   block_t *block_addr = needed_addr - sizeof(block_t);
  //
  //   uint32_t realsize = (uint32_t)block_addr - (uint32_t)(kernel_heap_top) +
  //                       OVERHEAD + rounded_size;
  //   block_t *ret;
  //   if (heap == &kernel_heap) {
  //     ret = ksbrk(realsize);
  //   } else {
  //     ret = usbrk(realsize);
  //   }
  //   assert(ret != NULL && "Heap is running out of space\n");
  //   if (!head_block) {
  //     *head = (uint32_t)block_addr;
  //   } else {
  //     tail_block->next = block_addr;
  //   }
  //
  //   ret->next = NULL;
  //   ret->nextfree = NULL;
  //   *tail = (uint32_t)block_addr;
  //
  //   /* After sbrk(), split the block into half [block_size  | the rest],
  //       * and put the rest into the tree.
  //       */
  //   block_addr->size = rounded_size;
  //   block_set_free(&(block_addr->size), 0);
  //   // Set the block allocated.
  //   // ptr = ptr + block_size - sizeof(uint32_t);
  //   // trailing_space = ptr;
  //   // *trailing_space = block_addr->size;
  //
  //   // Now, return it!
  //   return needed_addr;
  // }
}

/// @brief       Reallocates the given area of memory. It must be still allocated
///              and not yet freed with a call to free or realloc.
/// @param ptr
/// @param size
/// @return
static void *__do_realloc(vm_area_struct_t *heap, void *ptr, uint32_t size) {
  return NULL;
  // Get:
  // 1) First memory block.
  // static block_t *head = NULL;
  // 2) Last memory block.
  // static block_t *tail = NULL;
  // 3) All the memory blocks that are freed.
  // static block_t *freelist = NULL;

  // We will use these in writing.
  uint32_t *head = (uint32_t *)(heap->vm_start);
  uint32_t *tail = (uint32_t *)(heap->vm_start + sizeof(block_t *));
  uint32_t *freelist = (uint32_t *)(heap->vm_start + 2 * sizeof(block_t *));
  assert(head && tail && freelist && "Heap block lists point to null.");

  // We will use these others in reading.
  block_t *head_block = (block_t *)*head;
  block_t *tail_block = (block_t *)*tail;
  // block_t *first_free_block = (block_t *) *freelist;

  uint32_t *trailing_space = NULL;
  if (!ptr) {
    return kmalloc(size);
  }
  if (size == 0 && ptr != NULL) {
    kfree(ptr);

    return NULL;
  }
  uint32_t rounded_size = __ALIGN_UP(size, 16);
  uint32_t block_size = rounded_size + OVERHEAD;
  block_t *nextBlock;
  block_t *prevBlock;

  /* Shrink or expand?
    *
    * Shrink:
    * Now, we would just return the same address, later we may split this
    * block.
    *
    * Expand:
    * First, try if the actual size of the memory block is enough
    * to hold the current size.
    * Second, if not, try if merging the next block works.
    * Third, if none of the above works, malloc another block, move all the
    * data there, and then free the original block.
    */
  block_t *nptr = ptr - sizeof(block_t);
  nextBlock = block_get_next_block(nptr, tail);
  prevBlock = block_get_previous_block(nptr, head);
  if (nptr->size == size) {
    return ptr;
  }
  if (nptr->size < size) {
    // Expand, size of the block is just not enough.
    if (tail_block != nptr && block_is_free(nextBlock) &&
        (block_get_real_size(nptr->size) + OVERHEAD +
         block_get_real_size(nextBlock->size)) >= rounded_size) {
      // Merge with the next block, and return!
      // Change size to curr's size + OVERHEAD + next's size.
      block_remove_from_freelist(nextBlock, freelist);
      nptr->size = block_get_real_size(nptr->size) + OVERHEAD +
                   block_get_real_size(nextBlock->size);
      block_set_free(&(nptr->size), 0);
      trailing_space =
        (void *)nptr + sizeof(block_t) + block_get_real_size(nptr->size);
      *trailing_space = nptr->size;
      if (tail_block == nextBlock) {
        // Set it to tail for now, or we can reclaim it.
        *tail = (uint32_t)nptr;
      }
      return nptr + 1;
    }
    // Hey! Try merging with the previous block!
    else if (head_block != nptr && block_is_free(prevBlock) &&
             (block_get_real_size(nptr->size) + OVERHEAD +
              block_get_real_size(prevBlock->size)) >= rounded_size) {
      // db_print();
      uint32_t originalSize = block_get_real_size(nptr->size);
      // Hey! one more thing to do , copy data over to new block.
      block_remove_from_freelist(prevBlock, freelist);
      prevBlock->size =
        originalSize + OVERHEAD + block_get_real_size(prevBlock->size);
      block_set_free(&(prevBlock->size), 0);
      trailing_space = (void *)prevBlock + sizeof(block_t) +
                       block_get_real_size(prevBlock->size);
      *trailing_space = prevBlock->size;
      if (tail_block == nptr) {
        *tail = (uint32_t)prevBlock;
      }
      memcpy(prevBlock + 1, ptr, originalSize);

      return prevBlock + 1;
    }

    // Move to somewhere else.
    void *newplace = kmalloc(size);
    // Copy data over.
    memcpy(newplace, ptr, block_get_real_size(nptr->size));
    // Free original one
    kfree(ptr);

    return newplace;
  } else {
    /* Shrink/Do nothing, you can leave it as it's, but yeah... shrink
        * it What's left after shrinking the original block.
        */
    uint32_t rest = block_get_real_size(nptr->size) + OVERHEAD - block_size;
    if (rest < 8 + OVERHEAD)
      return ptr;

    nptr->size = block_size - OVERHEAD;
    block_set_free(&(nptr->size), 0);
    trailing_space =
      (void *)nptr + sizeof(block_t) + block_get_real_size(nptr->size);
    *trailing_space = nptr->size;
    /*
        * if(tail == nptr)
        * {
        *     ksbrk(-reclaimSize);
        *
        *     return ptr;
        * }
        */
    block_t *splitBlock = (void *)trailing_space + sizeof(uint32_t);

    /* Set the next, if the next of the next is also freed.. then merge!!
        * Wait... what if after merge, I get a much much more bigger block
        * than I even need? split again hahahahahah fuck!
        * Instead of spliting after merge, let's give splitBlock.
        */
    if (nextBlock && block_is_free(nextBlock)) {
      splitBlock->size = rest + block_get_real_size(nextBlock->size);
      block_set_free(&(splitBlock->size), 1);
      trailing_space = (void *)splitBlock + sizeof(block_t) +
                       block_get_real_size(splitBlock->size);
      *trailing_space = splitBlock->size;

      // Remove next block from freelist.
      block_remove_from_freelist(nextBlock, freelist);
      // This can be deleted when you correctly implemented malloc()
      if (tail_block == nextBlock) {
        *tail = (uint32_t)splitBlock;
      }
      // Add splitblock to freelist.
      block_add_to_freelist(splitBlock, freelist);

      return ptr;
    }
    // Separate!
    splitBlock->size = rest - OVERHEAD;
    block_set_free(&(splitBlock->size), 1);
    trailing_space = (void *)splitBlock + sizeof(block_t) +
                     block_get_real_size(splitBlock->size);
    *trailing_space = splitBlock->size;

    // Add this mo** f**r to the freelist!
    block_add_to_freelist(splitBlock, freelist);

    return ptr;
  }
}

/// @brief Deallocates previously allocated space.
/// @param heap Heap to which we return the allocated memory.
/// @param ptr  Pointer to the allocated memory.
static void __do_free(vm_area_struct_t *heap, void *ptr) {
  assert(ptr);

  // Get:
  // 1) First memory block.
  // static block_t *head = NULL;
  // 2) Last memory block.
  // static block_t *tail = NULL;
  // 3) All the memory blocks that are freed.
  // static block_t *freelist = NULL;

  // We will use these in writing.
  uint32_t *head = (uint32_t *)(heap->vm_start);
  uint32_t *tail = (uint32_t *)(heap->vm_start + sizeof(block_t *));
  uint32_t *freelist = (uint32_t *)(heap->vm_start + 2 * sizeof(block_t *));
  assert(head && tail && freelist && "Heap block lists point to null.");

  // We will use these others in reading.
  block_t *tail_block = (block_t *)*tail;

  block_t *curr = (block_t *)((char *)ptr - sizeof(block_t));

  block_t *prev = block_get_previous_block(curr, head);
  block_t *next = block_get_next_block(curr, tail);
  if (block_is_free(prev) && block_is_free(next)) {
    prev->size = block_get_real_size(prev->size) + 2 * OVERHEAD +
                 block_get_real_size(curr->size) +
                 block_get_real_size(next->size);
    block_set_free(&(prev->size), 1);

    prev->next = block_get_next_block(next, tail);

    // If next used to be tail, set prev = tail.
    if (tail_block == next) {
      *tail = (uint32_t)prev;
    }
    block_remove_from_freelist(next, freelist);
  } else if (block_is_free(prev)) {
    prev->size = block_get_real_size(prev->size) + OVERHEAD +
                 block_get_real_size(curr->size);
    block_set_free(&(prev->size), 1);

    prev->next = next;

    if (tail_block == curr) {
      *tail = (uint32_t)prev;
    }
  } else if (block_is_free(next)) {
    // Change size to curr's size + OVERHEAD + next's size.
    curr->size = block_get_real_size(curr->size) + OVERHEAD +
                 block_get_real_size(next->size);
    block_set_free(&(curr->size), 1);

    curr->next = block_get_next_block(next, tail);

    if (tail_block == next) {
      *tail = (uint32_t)curr;
    }
    block_remove_from_freelist(next, freelist);
    block_add_to_freelist(curr, freelist);
  } else {
    // Just mark curr freed.
    block_set_free(&(curr->size), 1);
    block_add_to_freelist(curr, freelist);
  }
}

// // TODO: rename in sys_brk
// void *umalloc(unsigned int size) {
//   // Get user heap segment structure.
//   struct vm_area_struct *heap_segment = find_user_heap();
//
//   // Allocate the segment if don't exist.
//   if (heap_segment == NULL) {
//     task_struct *current_task = kernel_get_current_process();
//     struct mm_struct *current_mm = current_task->mm;
//     current_mm->start_brk = create_segment(current_mm, UHEAP_INITIAL_SIZE);
//     current_mm->brk = current_mm->start_brk;
//     // Reserved space for:
//     // 1) First memory block.
//     // static block_t *head = NULL;
//     // 2) Last memory block.
//     // static block_t *tail = NULL;
//     // 3) All the memory blocks that are freed.
//     // static block_t *freelist = NULL;
//     current_mm->brk += 3 * sizeof(block_t *);
//     heap_segment = find_user_heap();
//   }
//
//   return __do_malloc(heap_segment, size);
// }
//
// // TODO: rename in sys_brk
// void ufree(void *p) {
//   // Get user heap segment structure.
//   struct vm_area_struct *heap_segment = find_user_heap();
//
//   // If the segment exists, user did a malloc previously.
//   if (heap_segment != NULL) {
//     __do_free(heap_segment, p);
//   }
// }

void *usbrk(int increment) {
  // task_struct *current_task = scheduler_get_current_process();
  // mm_struct_t *task_mm = current_task->mm;
  // uint32_t *heap_curr = &task_mm->brk;
  //
  // vm_area_struct_t *heap_segment = __find_user_heap();
  //
  // return __do_brk(heap_curr, heap_segment, increment);

  return NULL;
}

void *ksbrk(int increment) {
  return __do_brk(&kernel_heap_top, &kernel_heap, increment);
}

void *kmalloc(uint32_t sz) {
  return __do_malloc(&kernel_heap, sz);
}

// TODO: check!
void *kmalloc_align(size_t size) {
  return __do_malloc_align(&kernel_heap, size);
}

void *kcalloc(uint32_t num, uint32_t size) {
  void *ptr = kmalloc(num * size);
  if (ptr) {
    memset(ptr, 0, num * size);
  }

  return ptr;
}

// TODO: check!
void *krealloc(void *ptr, uint32_t size) {
  return __do_realloc(&kernel_heap, ptr, size);
}

void kfree(void *ptr) {
  __do_free(&kernel_heap, ptr);
}

// void *sys_brk(void *addr) {
//   // Get user heap segment structure.
//   vm_area_struct_t *heap_segment = __find_user_heap();
//
//   // Allocate the segment if don't exist.
//   if (heap_segment == NULL) {
//     task_struct *current_task = scheduler_get_current_process();
//     mm_struct_t *current_mm = current_task->mm;
//     current_mm->start_brk = create_vm_area(
//       current_mm, 0x40000000 /*FIXME! stabilize this*/, UHEAP_INITIAL_SIZE,
//       MM_RW | MM_PRESENT | MM_USER | MM_UPDADDR, GFP_HIGHUSER);
//     current_mm->brk = current_mm->start_brk;
//     // Reserved space for:
//     // 1) First memory block.
//     // static block_t *head = NULL;
//     // 2) Last memory block.
//     // static block_t *tail = NULL;
//     // 3) All the memory blocks that are freed.
//     // static block_t *freelist = NULL;
//     current_mm->brk += 3 * sizeof(block_t *);
//     heap_segment = __find_user_heap();
//   }
//
//   // If the address falls inside the memory region, call the free function,
//   // otherwise execute a malloc of the specified amount.
//   if (((uintptr_t)addr > heap_segment->vm_start) &&
//       ((uintptr_t)addr < heap_segment->vm_end)) {
//     __do_free(heap_segment, addr);
//     return NULL;
//   }
//   return __do_malloc(heap_segment, (uintptr_t)addr);
// }

void kheap_dump() {
  // 1) First memory block.
  // static block_t *head = NULL;
  // 2) Last memory block.
  // static block_t *tail = NULL;
  // 3) All the memory blocks that are freed.
  // static block_t *freelist = NULL;

  // We will use these in writing.
  uint32_t *head = (uint32_t *)(kernel_heap.vm_start);
  uint32_t *tail = (uint32_t *)(kernel_heap.vm_start + sizeof(block_t *));
  uint32_t *freelist =
    (uint32_t *)(kernel_heap.vm_start + 2 * sizeof(block_t *));
  assert(head && tail && freelist && "Heap block lists point to null.");

  // We will use these others in reading.
  block_t *head_block = (block_t *)*head;
  // block_t *tail_block = (block_t *) *tail;
  block_t *first_free_block = (block_t *)*freelist;

  if (!head_block) {
    // pr_debug("your heap is empty now\n");
    return;
  }

  // pr_debug("HEAP:\n");
  uint32_t total = 0;
  uint32_t total_overhead = 0;
  block_t *it = head_block;
  while (it) {
    dprintf("[%c] %12u (%12u)   from 0x%p to 0x%p\n",
            (block_is_free(it)) ? 'F' : 'A', block_get_real_size(it->size),
            it->size, it,
            (char *)it + OVERHEAD + block_get_real_size(it->size));
    total += block_get_real_size(it->size);
    total_overhead += OVERHEAD;
    it = it->next;
  }
  // pr_debug("\nTotal usable bytes   : %d", total);
  // pr_debug("\nTotal overhead bytes : %d", total_overhead);
  // pr_debug("\nTotal bytes          : %d", total + total_overhead);
  // pr_debug("\nFreelist: ");
  for (it = first_free_block; it != NULL; it = it->nextfree) {
    // pr_debug("(%p)->", it);
  }
  // pr_debug("\n\n");
  (void)total, (void)total_overhead;
}

void kheap_init(boot_info_t *boot_info) {
  // Kernel_heap_start.
  kernel_heap.vm_start = boot_info->heap_start;
  kernel_heap.vm_end = boot_info->heap_end;

  // Kernel_heap_start.
  kernel_heap_top = kernel_heap.vm_start;

  // FIXME!!
  // Set kernel_heap vm_area_struct info:
  //    kernel_heap.vm_next = NULL;
  //    kernel_heap.vm_mm = NULL;

  // Reserved space for:
  // 1) First memory block.
  // static block_t *head = NULL;
  // 2) Last memory block.
  // static block_t *tail = NULL;
  // 3) All the memory blocks that are freed.
  // static block_t *freelist = NULL;
  uint32_t allocated = 3 * sizeof(block_t *);
  // allocate page and map
  for (uint32_t i_virt = 0; i_virt < allocated; i_virt += PAGE_SIZE) {
    vmm_create_page((uint32_t)kernel_heap_top + i_virt, PML_USER_ACCESS);
  }
  memset((void *)kernel_heap_top, 0, allocated);
  kernel_heap_top += allocated;
}
