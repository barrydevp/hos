#include <kernel/memory/mmu.h>
#include <kernel/memory/vmm.h>

/**
 * @brief Allocate space in the kernel virtual heap.
 *
 * Called by the kernel heap allocator to obtain space for new heap allocations.
 *
 * @warning Not to be confused with sys_sbrk
 *
 * @param bytes Bytes to allocate. Must be a multiple of PAGE_SIZE.
 * @returns The previous address of the break point, after which @p bytes may now be used.
 */
// void *sbrk(size_t bytes) {
//   if (!heap_start) {
//     // arch_fatal_prepare();
//     // printf("sbrk: Called before heap was ready.\n");
//     // arch_dump_traceback();
//     // arch_fatal();
//   }
//
//   if (!bytes) {
//     /* Skip lock acquisition if we just wanted to know where the break was. */
//     return heap_start;
//   }
//
//   if (bytes & PAGE_LOW_MASK) {
//     // arch_fatal_prepare();
//     // printf("sbrk: Size must be multiple of 4096, was %#zx\n", bytes);
//     // arch_dump_traceback();
//     // arch_fatal();
//   }
//
//   if (bytes > 0x1F00000) {
//     // arch_fatal_prepare();
//     // printf("sbrk: Size must be within a reasonable bound, was %#zx\n", bytes);
//     // arch_dump_traceback();
//     // arch_fatal();
//   }
//
//   // spin_lock(kheap_lock);
//   void *out = heap_start;
//
//   for (uintptr_t p = (uintptr_t)out; p < (uintptr_t)out + bytes;
//        p += PAGE_SIZE) {
//     union PML *page = vmm_get_page(p, VMM_FLAG_WRITABLE | VMM_FLAG_KERNEL);
//   }
//
//   //memset(out, 0xAA, bytes);
//
//   heap_start += bytes;
//   // spin_unlock(kheap_lock);
//   return out;
// }
