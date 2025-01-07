#include <kernel/memory/vmm.h>
#include <kernel/memory/mmu.h>
#include <kernel/string.h>

#include <kernel/printf.h>

#define get_page_directory_index(x)   (((x) >> 22) & 0x3ff)
#define get_page_table_entry_index(x) (((x) >> 12) & 0x3ff)
#define get_aligned_address(x)        (x & ~0xfff)
#define is_page_enabled(x)            (x & 0x1)

/* Initial memory maps loaded by boostrap */
#define _pagemap __attribute__((aligned(PAGE_SIZE))) = { 0 }
// [0] = init PDE (page directory table) for boot and will become kernel PDE, [1..] = identity+boot+kernel PTE (page tables)
union PML init_page_region[1 + KERNEL_INIT_NPTE][PAGE_TABLE_SIZE] _pagemap;

/// The mm_struct of the kernel.
// static mm_struct_t k_mm _pagemap;
page_directory_t *k_pdir;

/* keep track the cur_pdir */
// TODO: move this to k_core->pdir
page_directory_t *cur_pdir;

/* kernel heap */
char *heap_start = NULL;

static inline void vmm_flush_tlb_entry(uintptr_t addr) {
  asm volatile("invlpg (%0)" ::"r"(addr) : "memory");
}

/**
 * @brief Mark a virtual address's mappings as invalid in the TLB.
 *
 * Generally should be called when a mapping is relinquished, as this is what
 * the TLB caches, but is also called in a bunch of places where we're just mapping
 * new pages...
 *
 * @param addr Virtual address in the current address space to invalidate.
 */
void vmm_invalidate(uintptr_t addr) {
  asm volatile("invlpg (%0)" ::"r"(addr) : "memory");
  // arch_tlb_shootdown(addr);
}

void paging_flush_tlb_single(uintptr_t addr) {
  asm volatile("invlpg (%0)" ::"r"(addr) : "memory");
}

static inline void vmm_page_set_flags(union PML *page, uint32_t flags) {
  page->raw |= PAGE_LOW_MASK & flags;
}

/**
 * @brief Set the flags for a page in ptable, and allocate a page for it if needed.
 *
 * Sets the page bits based on the the value of @p flags.
 * If @p page->pte.frame is unset, a new frame will be allocated.
 * Don't use page->pte.present because it determine weather page is in memory
 * or swapped out, not unallocated page.
 */
void vmm_page_allocate(union PML *page, uint32_t flags) {
  if (page->ptbits.present == 0) {
    uint32_t index     = pmm_allocate_frame();
    page->ptbits.frame = index;
  }
  vmm_page_set_flags(page, flags);
}

/**
 * @brief Map the given page to the requested physical address.
 */
void vmm_page_map_addr(union PML *page, uint32_t flags, uintptr_t physAddr) {
  pmm_frame_seta(physAddr);
  page->ptbits.frame = physAddr >> FRAME_SHIFT;
  vmm_page_set_flags(page, flags);
}

/**
 * @brief Set the flags for a ptable in pdirectory, and allocate a new ptable if needed.
 *
 * Sets the ptable bits based on the the value of @p flags.
 * If @p page->pte.frame is unset, a new frame will be allocated.
 * Don't use page->pte.present because it determine weather page is in memory
 * or swapped out, not unallocated page.
 */
void vmm_pde_allocate(union PML *pde, uint32_t flags) {
  if (pde->pdbits.present == 0) {
    uint32_t index    = pmm_allocate_frame();
    pde->pdbits.frame = index;
  }
  vmm_page_set_flags(pde, flags);
}

page_directory_t *vmm_get_kernel_directory(void) {
  return k_pdir;
}

/**
 * @brief Get current page directory virtual address
 */
page_directory_t *vmm_get_directory(void) {
  return cur_pdir;
}

/**
 * @brief Get current page directory recursive virtual address
 */
page_directory_t *vmm_r_get_directory(void) {
  // It's easy because we use recursive mapping, the last entry PDE[1023]
  return (page_directory_t *)PAGE_DIRECTORY_VIRT;
}

/**
 * @brief Get page table recursive address of provided virtAddr page
 */
page_table_t *vmm_r_get_ptable(uintptr_t virtAddr) {
  // It's easy because we use recursive mapping, the last entry PTE[1023]
  return (page_table_t *)PAGE_TABLE_BASE + PDE_INDEX(virtAddr);
}

static inline uintptr_t __get_cr3(void) {
  uintptr_t cr3;
  asm volatile("mov %%cr3, %0" : "=r"(cr3));
  return (cr3);
}

static inline void __set_cr3(uintptr_t cr3) {
  asm volatile("mov %0, %%cr3" : : "r"(cr3));
}

/**
 * @brief Switch the active page directory for kernel.
 *
 * Generally called during task(process) creation and switching to change
 * the active page directory of a core. Updates @c k_core->current_pdir.
 *
 * x86: Loads a given Page Directory into CR3.
 *
 * @param new_pdir Either the physical address or the shadow mapping virtual address
 *                 of the new PML4 directory to switch into, general obtained from
 *                 a process struct; if NULL is passed, the initial kernel directory
 *                 will be used and no userspace mappings will be present.
 */
void vmm_switch_directory(page_directory_t *new_pdir) {
  if (!new_pdir)
    new_pdir = k_pdir;

  cur_pdir = new_pdir;
  // k_core->current_pml = new_pdir;
  uint32_t pdir_phy_addr = new_pdir->entries[1023].raw;
  dprintf("Switch page directory to: 0x%p\n", pdir_phy_addr & FRAME_MASK);

  __set_cr3(pdir_phy_addr & FRAME_MASK);
}

/// @brief Get physical address (frame address) mapping of virtual address.
/// @param virtAddr Virtual address.
/// @return The frame address.
uintptr_t vmm_r_get_phy_addr(uintptr_t virtAddr) {
  page_table_t *pt  = vmm_r_get_ptable(virtAddr);
  uint32_t pt_entry = (virtAddr >> PAGE_SHIFT) & ENTRY_MASK;

  uintptr_t phyAddr = pt->pages[pt_entry].raw;

  return (phyAddr & ~0xfff) | (virtAddr & 0xfff);
}

page_directory_t *vmm_clone_pdir(page_directory_t *from) {
  return NULL;
  // /* Clone the current PMLs... */
  // if (!from)
  //   from = cur_pdir;
  //
  // page_directory_t *new_pdir = sbrk(PAGE_SIZE);
  //
  // /* Setup PD */
  // // Zero bottom user half
  // memset(&new_pdir->entries[0], 0, 768 * sizeof(union PML));
  // // Copy higher kernel half
  // memcpy(&new_pdir[768], &from[768], (1024 - 768) * sizeof(union PML));
  //
  // /* Setup PTs */
  // // for (size_t i = 0; i < 768; i++) {
  // //   if (from[i].pdbits.present) {
  // //     union PML *new_pt = sbrk(PAGE_SIZE);
  // //   }
  // // }
  //
  // return new_pdir;
}

/**
 * @brief Obtain the page entry for a virtual address (it is PTE)
 *
 * Digs into the current page directory to obtain the page entry
 * for a requested address @p virtAddr. If new intermediary directories
 * need to be allocated and @p flags has @c MMU_GET_MAKE set, they
 * will be allocated with the user access bits set. Otherwise,
 * NULL will be returned. If the requested virtual address is within
 * a large page, NULL will be returned.
 *
 * @param virtAddr Canonical virtual address offset.
 * @param flags See @c MMU_GET_MAKE
 * @returns the requested page entry, or NULL if doing so required allocating
 *          an intermediary paging level and @p flags did not have @c MMU_GET_MAKE set.
 */
union PML *vmm_create_page(uintptr_t virtAddr, uint32_t flags) {
  dprintf("create_page(0x%p)\n", virtAddr);

  virtAddr           = __ALIGN_DOWN(virtAddr, PAGE_SIZE);
  uintptr_t pageAddr = virtAddr >> PAGE_SHIFT;
  uint32_t pd_entry  = (pageAddr >> 10) & ENTRY_MASK;
  uint32_t pt_entry  = (pageAddr)&ENTRY_MASK;

  page_directory_t *pd = vmm_r_get_directory();
  page_table_t *pt     = vmm_r_get_ptable(virtAddr);

  /* Setup page table */
  if (!pd->entries[pd_entry].pdbits.present) {
    vmm_pde_allocate(&pd->entries[pd_entry], flags);
    /* zero it */
    memset(pt, 0, PAGE_SIZE);
  }

  if (pd->entries[pd_entry].pdbits.size) {
    dprintf("Warning: Tried to get page for a 4MiB page!\n");
    return NULL;
  }

  if (!pt->pages[pt_entry].ptbits.present) {
    vmm_page_allocate(&pt->pages[pt_entry], flags);
    /* zero it */
    memset((void *)virtAddr, 0, PAGE_SIZE);
  }
  vmm_page_set_flags(&pt->pages[pt_entry], flags);

  return (union PML *)&pt->pages[pt_entry];
}

/**
 * @brief Obtain the page entry for a virtual address (it is PTE)
 *
 * Digs into the current page directory to obtain the page entry
 * for a requested address @p virtAddr. If new intermediary directories
 * need to be allocated and @p flags has @c MMU_GET_MAKE set, they
 * will be allocated with the user access bits set. Otherwise,
 * NULL will be returned. If the requested virtual address is within
 * a large page, NULL will be returned.
 *
 * @param virtAddr Canonical virtual address offset.
 * @param flags See @c MMU_GET_MAKE
 * @returns the requested page entry, or NULL if doing so required allocating
 *          an intermediary paging level and @p flags did not have @c MMU_GET_MAKE set.
 */
union PML *vmm_get_page(uintptr_t virtAddr) {
  virtAddr           = __ALIGN_DOWN(virtAddr, PAGE_SIZE);
  uintptr_t pageAddr = virtAddr >> PAGE_SHIFT;
  uint32_t pd_entry  = (pageAddr >> 10) & ENTRY_MASK;
  uint32_t pt_entry  = (pageAddr)&ENTRY_MASK;

  page_directory_t *pd = vmm_r_get_directory();
  page_table_t *pt     = vmm_r_get_ptable(virtAddr);

  /* Setup page table */
  if (!pd->entries[pd_entry].pdbits.present) {
    goto _noentry;
  }

  if (pd->entries[pd_entry].pdbits.size) {
    dprintf("Warning: Tried to get page for a 4MiB page!\n");
    return NULL;
  }

  if (!pt->pages[pt_entry].ptbits.present) {
    goto _noentry;
  }

  return (union PML *)&pt->pages[pt_entry];

_noentry:
  dprintf("no entry for requested page\n");
  return NULL;
}

union PML *vmm_map_page(uintptr_t virtAddr, uintptr_t physAddr,
                        uint32_t flags) {
  dprintf("map_page(0x%p, 0x%p)\n", virtAddr, physAddr);
  virtAddr           = __ALIGN_DOWN(virtAddr, PAGE_SIZE);
  uintptr_t pageAddr = virtAddr >> PAGE_SHIFT;
  uint32_t pd_entry  = (pageAddr >> 10) & ENTRY_MASK;
  uint32_t pt_entry  = (pageAddr)&ENTRY_MASK;

  page_directory_t *pd = vmm_get_directory();
  page_table_t *pt     = vmm_r_get_ptable(virtAddr);

  /* Setup page table */
  if (!pd->entries[pd_entry].pdbits.present) {
    vmm_pde_allocate(&pd->entries[pd_entry], flags);
    /* zero it */
    memset(pt, 0, PAGE_SIZE);
  }

  if (pd->entries[pd_entry].pdbits.size) {
    // printf("Warning: Tried to get page for a 4MiB page!\n");
    return NULL;
  }

  if (pt->pages[pt_entry].ptbits.present) {
    dprintf("This page is currently in used!\n");
  }

  vmm_page_map_addr(&pt->pages[pt_entry], flags, physAddr);

  return (union PML *)&pt->pages[pt_entry];
}

void vmm_map_range(uintptr_t virtAddr, uintptr_t physAddr, uint32_t size,
                   uint32_t flags) {
  uintptr_t startAddr = __ALIGN_DOWN(virtAddr, PAGE_SIZE);
  uintptr_t endAddr   = __ALIGN_UP(virtAddr + size, PAGE_SIZE);
  physAddr            = __ALIGN_DOWN(physAddr, PAGE_SIZE);
  for (; startAddr < endAddr; startAddr += PAGE_SIZE, physAddr += PAGE_SIZE) {
    vmm_map_page(startAddr, physAddr, flags);
  }
}

void vmm_unmap_page(uintptr_t virtAddr) {
  virtAddr           = __ALIGN_DOWN(virtAddr, PAGE_SIZE);
  uintptr_t pageAddr = virtAddr >> PAGE_SHIFT;
  uint32_t pd_entry  = (pageAddr >> 10) & ENTRY_MASK;
  uint32_t pt_entry  = (pageAddr)&ENTRY_MASK;

  page_directory_t *pd = vmm_get_directory();
  page_table_t *pt     = vmm_r_get_ptable(virtAddr);

  /* Setup page table */
  if (!pd->entries[pd_entry].pdbits.present) {
    goto __return;
  }

  if (!pt->pages[pt_entry].ptbits.present) {
    goto __return;
  }

  pt->pages[pt_entry].raw = 0x0u;

__return:
  vmm_invalidate(virtAddr);
}

void vmm_free_page(uintptr_t virtAddr) {
  virtAddr           = __ALIGN_DOWN(virtAddr, PAGE_SIZE);
  uintptr_t pageAddr = virtAddr >> PAGE_SHIFT;
  uint32_t pd_entry  = (pageAddr >> 10) & ENTRY_MASK;
  uint32_t pt_entry  = (pageAddr)&ENTRY_MASK;

  page_directory_t *pd = vmm_get_directory();
  page_table_t *pt     = vmm_r_get_ptable(virtAddr);

  /* Setup page table */
  if (!pd->entries[pd_entry].pdbits.present) {
    return;
  }

  if (!pt->pages[pt_entry].ptbits.present) {
    return;
  }

  pmm_free_frame(pt->pages[pt_entry].raw);
  pt->pages[pt_entry].raw = 0x0u;
  vmm_invalidate(virtAddr);
}

void vmm_unmap_range(uintptr_t virtAddr, uint32_t size) {
  // unmap page without free frame
  uintptr_t startAddr = __ALIGN_UP(virtAddr, PAGE_SIZE);
  uintptr_t endAddr   = __ALIGN_DOWN(virtAddr + size, PAGE_SIZE);
  for (; startAddr < endAddr; startAddr += PAGE_SIZE) {
    vmm_unmap_page(startAddr);
  }
}

void vmm_allocate_range(uintptr_t virtAddr, uint32_t size, uint32_t flags) {
  uintptr_t startAddr = __ALIGN_DOWN(virtAddr, PAGE_SIZE);
  uintptr_t endAddr   = __ALIGN_UP(virtAddr + size, PAGE_SIZE);
  for (; startAddr < endAddr; startAddr += PAGE_SIZE) {
    vmm_create_page(startAddr, flags);
  }
}

void vmm_deallocate_range(uintptr_t virtAddr, uint32_t size) {
  // free page and its associated frame
  uintptr_t startAddr = __ALIGN_UP(virtAddr, PAGE_SIZE);
  uintptr_t endAddr   = __ALIGN_DOWN(virtAddr + size, PAGE_SIZE);
  for (; startAddr < endAddr; startAddr += PAGE_SIZE) {
    vmm_free_page(startAddr);
  }

  // free ptable and its associated frame
  uint32_t pd_i        = (__ALIGN_UP(virtAddr, PAGE_SIZE) >> 20) & ENTRY_MASK;
  uint32_t pd_e        = pd_i + (size >> 20);
  page_directory_t *pd = vmm_get_directory();
  for (; pd_i < pd_e; ++pd_i) {
    if (pd->entries[pd_i].pdbits.present) {
      pmm_free_frame(pd->entries[pd_i].raw);
      pd->entries[pd_i].raw = 0x0u;
    }
  }
}

void vmm_init(struct boot_info_t *boot_info) {
  // initialize page table directory

  // allocate page directory
  k_pdir               = (page_directory_t *)init_page_region[0];
  cur_pdir             = k_pdir;
  uintptr_t k_phy_pdir = __get_cr3();

  /** unmap the identity mapping first 4MB */
  // TODO: currently we cannot unmap this because multiboot
  // data is mapped at it's space
  // k_pdir->entries[0].raw = 0;

  /* Preallocate ptable for higher half kernel and set KERNEL ACCESS protected */
  for (int i = KERNEL_PDE_START_IDX + KERNEL_INIT_NPTE; i < 1023; ++i) {
    vmm_pde_allocate(&k_pdir->entries[i], PML_KERNEL_ACCESS);
  }

  /* Recursive mapping */
  k_pdir->entries[1023].raw = (k_phy_pdir & PAGE_MASK) | PML_KERNEL_ACCESS;

  /* Allocate page for pmm used region */
  // FIXME: another approach in future
  uint32_t pmm_used  = PAGE_ALIGN(128 * KB); // for bitmap
  uint32_t pmm_start = FRAME_ALIGN(boot_info->kernel_end);
  for (uint32_t i_virt = 0; i_virt < pmm_used; i_virt += PAGE_SIZE) {
    vmm_create_page(pmm_start + i_virt, PML_KERNEL_ACCESS);
  }

  /* Map page for video region */
  uint32_t video_size =
    PAGE_ALIGN(boot_info->video_end - boot_info->video_start);
  vmm_map_range(boot_info->video_start, boot_info->video_phy_start, video_size,
                PML_USER_ACCESS);

  // swithc to k_pdir, actually this is redundant
  vmm_switch_directory(k_pdir);
}
