#include <kernel/memory/vmm.h>
#include <kernel/memory/mmu.h>
#include <kernel/string.h>

#define get_page_directory_index(x) (((x) >> 22) & 0x3ff)
#define get_page_table_entry_index(x) (((x) >> 12) & 0x3ff)
#define get_aligned_address(x) (x & ~0xfff)
#define is_page_enabled(x) (x & 0x1)

/* Initial memory maps loaded by boostrap */
#define _pagemap __attribute__((aligned(PAGE_SIZE))) = { 0 }
// [0] = init PDT for boot and will become kernel PDT, [1] = identity+boot+kernel PT
union PML init_page_region[1 + KERNEL_INIT_NPDE][PAGE_TABLE_SIZE] _pagemap;
page_directory_t *k_pdir;
// #define k_pdir init_page_region[0]

/* keep track the cur_pdir */
// TODO: move this to k_core->pdir
page_directory_t *cur_pdir;

/* kernel heap */
char *heap_start = NULL;

static inline void vmm_flush_tlb_entry(uint32_t addr) {
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

static inline void vmm_page_set_flags(union PML *page, uint32_t flags) {
  page->raw |= PAGE_LOW_MASK | flags;
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
  if (page->ptbits.frame == 0) {
    uint32_t index = pmm_allocate_frame();
    page->ptbits.frame = index;
  }
  vmm_page_set_flags(page, flags);
}

/**
 * @brief Map the given page to the requested physical address.
 */
void vmm_page_map_address(union PML *page, uint32_t flags, uintptr_t physAddr) {
  pmm_frame_seta(physAddr);
  page->ptbits.frame = physAddr >> PAGE_SHIFT;
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
void vmm_ptable_allocate(union PML *pde, uint32_t flags) {
  if (pde->pdbits.frame == 0) {
    uint32_t index = pmm_allocate_frame();
    pde->pdbits.frame = index;
  }
  vmm_page_set_flags(pde, flags);
}

/// @brief create a region of virtual memory
void vmm_create_region(page_directory_t *pgd, uint32_t virt_start, size_t size,
                       uint32_t flags) {
  // for (uint32_t page_n = 0; page_n < (size >> PAGE_SHIFT); page_n++) {
  //   vmm_create_page()
  // }
}

page_directory_t *vmm_get_kernel_directory(void) {
  return k_pdir;
}

/**
 * @brief Get current page directory virtual address
 */
page_directory_t *vmm_get_directory(void) {
  // It's easy because we use recursive mapping
  return (page_directory_t *)0xFFFFF000;
  // return cur_pdir;
}

/**
 * @brief Get page table recursive address of provided virtAddr page
 */
page_table_t *vmm_r_get_ptable(uintptr_t virtAddr) {
  // It's easy because we use recursive mapping
  return (page_table_t *)PAGE_TABLE_BASE + PDE_INDEX(virtAddr);
}

static inline uintptr_t __get_cr3(void) {
  uintptr_t cr3;
  asm volatile("mov %%cr3, %0" : "=r"(cr3));
  return (cr3);
}

static inline void __set_cr3(uintptr_t cr3) {
  asm volatile("movl %0, %%cr3" : : "r"(cr3));
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
void vmm_set_directory(page_directory_t *new_pdir, uint32_t phy_addr) {
  if (!new_pdir)
    new_pdir = k_pdir;

  cur_pdir = new_pdir;
  // k_core->current_pml = new_pdir;
  // uintptr_t pdir_frame = (uintptr_t)&new_pdir->entries[1023];

  // asm volatile("movl %0, %%cr3" : : "r"(pdir_frame & PAGE_MASK));
  __set_cr3(phy_addr & PAGE_MASK);
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
union PML *vmm_create_page(uintptr_t virtAddr, int flags) {
  uintptr_t pageAddr = virtAddr >> PAGE_SHIFT;
  unsigned int pd_entry = (pageAddr >> 10) & ENTRY_MASK;
  unsigned int pt_entry = (pageAddr)&ENTRY_MASK;

  page_directory_t *pd = vmm_get_directory();
  page_table_t *pt = vmm_r_get_ptable(virtAddr);

  /* Setup page table */
  if (!pd->entries[pd_entry].pdbits.present) {
    vmm_ptable_allocate(&pd->entries[pd_entry], flags);
    /* zero it */
    memset(pt, 0, PAGE_SIZE);
  }

  if (pd->entries[pd_entry].pdbits.size) {
    // printf("Warning: Tried to get page for a 4MiB page!\n");
    return NULL;
  }

  if (!pt->pages[pt_entry].pdbits.frame) {
    vmm_page_allocate(&pt->pages[pt_entry], flags);
    /* zero it */
    memset((void *)virtAddr, 0, PAGE_SIZE);
  }

  return (union PML *)&pt->pages[pt_entry];
}

// void vmm_alloc_ptable(struct pdirectory *va_dir, uint32_t index) {
//   if (is_page_enabled(va_dir->m_entries[index]))
//     return;
//
//   uint32_t pa_table = (uint32_t)pmm_alloc_block();
//   va_dir->m_entries[index] = pa_table | I86_PDE_PRESENT | I86_PDE_WRITABLE;
// }
//
// void vmm_init_and_map(struct pdirectory *va_dir, uint32_t vaddr,
//                       uint32_t paddr) {
//   uint32_t pa_table = (uint32_t)pmm_alloc_block();
//   struct ptable *va_table = (struct ptable *)(pa_table + KERNEL_HIGHER_HALF);
//   memset(va_table, 0, sizeof(struct ptable));
//
//   uint32_t ivirtual = vaddr;
//   uint32_t iframe = paddr;
//
//   for (int i = 0; i < 1024;
//        ++i, ivirtual += PMM_FRAME_SIZE, iframe += PMM_FRAME_SIZE) {
//     pt_entry *entry =
//       &va_table->m_entries[get_page_table_entry_index(ivirtual)];
//     *entry = iframe | I86_PTE_PRESENT | I86_PTE_WRITABLE;
//     pmm_mark_used_addr(iframe);
//   }
//
//   pd_entry *entry = &va_dir->m_entries[get_page_directory_index(vaddr)];
//   *entry = pa_table | I86_PDE_PRESENT | I86_PDE_WRITABLE;
// }
//
// void vmm_paging(struct pdirectory *va_dir, uint32_t pa_dir) {
//   _current_dir = va_dir;
//
//   __asm__ __volatile__("mov %0, %%cr3           \n"
//                        "mov %%cr4, %%ecx        \n"
//                        "and $~0x00000010, %%ecx \n"
//                        "mov %%ecx, %%cr4        \n"
//                        "mov %%cr0, %%ecx        \n"
//                        "or $0x80000000, %%ecx   \n"
//                        "mov %%ecx, %%cr0        \n" ::"r"(pa_dir));
// }
//
// void pt_entry_add_attrib(pt_entry *e, uint32_t attr) {
//   *e |= attr;
// }
//
// void pt_entry_set_frame(pt_entry *e, uint32_t addr) {
//   *e = (*e & ~I86_PTE_FRAME) | addr;
// }
//
// void pd_entry_add_attrib(pd_entry *e, uint32_t attr) {
//   *e |= attr;
// }
//
// void pd_entry_set_frame(pd_entry *e, uint32_t addr) {
//   *e = (*e & ~I86_PDE_FRAME) | addr;
// }
//
// uint32_t vmm_get_physical_address(uint32_t vaddr, bool is_page) {
//   uint32_t *table =
//     (uint32_t *)((char *)PAGE_TABLE_BASE +
//                  get_page_directory_index(vaddr) * PMM_FRAME_SIZE);
//   uint32_t tindex = get_page_table_entry_index(vaddr);
//   uint32_t paddr = table[tindex];
//   if (is_page)
//     return paddr;
//   else
//     return (paddr & ~0xfff) | (vaddr & 0xfff);
// }
//
// struct pdirectory *vmm_create_address_space(struct pdirectory *current) {
//   char *aligned_object = kalign_heap(PMM_FRAME_SIZE);
//   // NOTE: MQ 2019-11-24 page directory, page table have to be aligned by 4096
//   struct pdirectory *va_dir = kcalloc(1, sizeof(struct pdirectory));
//   if (aligned_object)
//     kfree(aligned_object);
//
//   for (int i = 768; i < 1023; ++i)
//     va_dir->m_entries[i] =
//       vmm_get_physical_address(PAGE_TABLE_BASE + i * PMM_FRAME_SIZE, true);
//
//   // NOTE: MQ 2019-11-26 Recursive paging for new page directory
//   va_dir->m_entries[1023] = vmm_get_physical_address((uint32_t)va_dir, true);
//
//   return va_dir;
// }
//
// struct pdirectory *vmm_get_directory() {
//   return _current_dir;
// }
//
// /*
//   NOTE: MQ 2019-05-08
//   cr3 -> 0xsomewhere (physical address): | 0 | 1 | ... | 1023 | (1023th pd's value is 0xsomewhere)
//   When translating a virtual address vAddr:
//     + de is ath page directory index (vAddr >> 22)
//     + te is bth page table index (vAddr >> 12 & 0x1111111111)
//   If pd[de] is not present, getting 4KB free from pmm_alloc_block() and assigning it at page directory entry
//
//   mmus' formula: pd = cr3(); pt = *(pd+4*PDX); page = *(pt+4*PTX)
//   The issue is we cannot directly access *(pd+4*PDX) and access/modify a page table entry because everything(kernel) is now a virtual address
//   To overcome the egg-chicken's issue, we set 1023pde=cr3
//
//   For example:
//   0xFFC00000 + de * 0x1000 is mapped to pd[de] (physical address). As you know virtual <-> physical address is 4KB aligned
//   0xFFC00000 + de * 0x1000 + te * 0x4 is mapped to pd[de] + te * 0x4 (this is what mmu will us to translate vAddr)
//   0xFFC00000 + de * 0x1000 + te * 0x4 = xxx <-> *(pt+4*ptx) = xxx
// */
// void vmm_map_address(struct pdirectory *va_dir, uint32_t virt, uint32_t phys,
//                      uint32_t flags) {
//   if (virt != PAGE_ALIGN(virt))
//     dlog("0x%x is not page aligned", virt);
//
//   if (!is_page_enabled(va_dir->m_entries[get_page_directory_index(virt)]))
//     vmm_create_page_table(va_dir, virt, flags);
//
//   uint32_t *table =
//     (uint32_t *)((char *)PAGE_TABLE_BASE +
//                  get_page_directory_index(virt) * PMM_FRAME_SIZE);
//   uint32_t tindex = get_page_table_entry_index(virt);
//
//   table[tindex] = phys | flags;
//   vmm_flush_tlb_entry(virt);
// }
//
// void vmm_create_page_table(struct pdirectory *va_dir, uint32_t virt,
//                            uint32_t flags) {
//   if (is_page_enabled(va_dir->m_entries[get_page_directory_index(virt)]))
//     return;
//
//   uint32_t pa_table = (uint32_t)pmm_alloc_block();
//
//   va_dir->m_entries[get_page_directory_index(virt)] = pa_table | flags;
//   vmm_flush_tlb_entry(virt);
//
//   memset((char *)PAGE_TABLE_BASE +
//            get_page_directory_index(virt) * PMM_FRAME_SIZE,
//          0, sizeof(struct ptable));
// }
//
// void vmm_unmap_address(struct pdirectory *va_dir, uint32_t virt) {
//   if (virt != PAGE_ALIGN(virt))
//     dlog("0x%x is not page aligned", virt);
//
//   if (!is_page_enabled(va_dir->m_entries[get_page_directory_index(virt)]))
//     return;
//
//   struct ptable *pt =
//     (struct ptable *)(PAGE_TABLE_BASE +
//                       get_page_directory_index(virt) * PMM_FRAME_SIZE);
//   uint32_t pte = get_page_table_entry_index(virt);
//
//   if (!is_page_enabled(pt->m_entries[pte]))
//     return;
//
//   pt->m_entries[pte] = 0;
//   vmm_flush_tlb_entry(virt);
// }
//
// void vmm_unmap_range(struct pdirectory *va_dir, uint32_t vm_start,
//                      uint32_t vm_end) {
//   assert(PAGE_ALIGN(vm_start) == vm_start);
//   assert(PAGE_ALIGN(vm_end) == vm_end);
//
//   for (uint32_t addr = vm_start; addr < vm_end; addr += PMM_FRAME_SIZE)
//     vmm_unmap_address(va_dir, addr);
// }
//
// struct pdirectory *vmm_fork(struct pdirectory *va_dir) {
//   struct pdirectory *forked_dir = vmm_create_address_space(va_dir);
//   char *aligned_object = kalign_heap(PMM_FRAME_SIZE);
//   uint32_t heap_current = (uint32_t)sbrk(0);
//
//   // NOTE: MQ 2019-12-15 Any heap changes via malloc is forbidden
//   for (int ipd = 0; ipd < 768; ++ipd)
//     if (is_page_enabled(va_dir->m_entries[ipd])) {
//       struct ptable *forked_pt = (struct ptable *)heap_current;
//       uint32_t forked_pt_paddr = (uint32_t)pmm_alloc_block();
//       vmm_map_address(va_dir, (uint32_t)forked_pt, forked_pt_paddr,
//                       I86_PTE_PRESENT | I86_PTE_WRITABLE | I86_PTE_USER);
//       memset(forked_pt, 0, sizeof(struct ptable));
//
//       heap_current += sizeof(struct ptable);
//       struct ptable *pt =
//         (struct ptable *)(PAGE_TABLE_BASE + ipd * PMM_FRAME_SIZE);
//       for (int ipt = 0; ipt < PAGES_PER_TABLE; ++ipt) {
//         if (is_page_enabled(pt->m_entries[ipt])) {
//           char *pte = (char *)heap_current;
//           char *forked_pte = pte + PMM_FRAME_SIZE;
//           heap_current = (uint32_t)(forked_pte + PMM_FRAME_SIZE);
//           uint32_t forked_pte_paddr = (uint32_t)pmm_alloc_block();
//
//           vmm_map_address(va_dir, (uint32_t)pte, pt->m_entries[ipt],
//                           I86_PTE_PRESENT | I86_PTE_WRITABLE | I86_PTE_USER);
//           vmm_map_address(va_dir, (uint32_t)forked_pte, forked_pte_paddr,
//                           I86_PTE_PRESENT | I86_PTE_WRITABLE | I86_PTE_USER);
//
//           memcpy(forked_pte, pte, PMM_FRAME_SIZE);
//
//           vmm_unmap_address(va_dir, (uint32_t)pte);
//           vmm_unmap_address(va_dir, (uint32_t)forked_pte);
//
//           forked_pt->m_entries[ipt] = forked_pte_paddr | I86_PTE_PRESENT |
//                                       I86_PTE_WRITABLE | I86_PTE_USER;
//         }
//       }
//       vmm_unmap_address(va_dir, (uint32_t)forked_pt);
//       forked_dir->m_entries[ipd] = forked_pt_paddr | I86_PDE_PRESENT |
//                                    I86_PDE_WRITABLE | I86_PDE_USER;
//     }
//
//   if (aligned_object)
//     kfree(aligned_object);
//   return forked_dir;
// }

void vmm_init(struct boot_info_t *boot_info) {
  // initialize page table directory
  // log("VMM: Initializing");

  // heap_start = (char *)boot_info->heap_start;
  // uint32_t lowmem_current = boot_info->lowmem_current;

  // allocate page directory
  k_pdir = (page_directory_t *)init_page_region[0];
  uintptr_t k_phy_pdir = __get_cr3();
  // lowmem_current += sizeof(page_directory_t);

  /* Preallocate ptable for higher half kernel and set KERNEL ACCESS protected */
  for (int i = KERNEL_PDE_START + KERNEL_INIT_NPDE; i < 1023; ++i) {
    vmm_ptable_allocate(&k_pdir->entries[i], PML_KERNEL_ACCESS);
  }

  // log("VMM: Setup recursive page directory");
  /* Recursive mapping */
  k_pdir->entries[1023].raw = (k_phy_pdir & PAGE_MASK) | PML_KERNEL_ACCESS;

  /* Allocate page for lowmem + kernel stack region */
  uint32_t lowmem_size = (boot_info->stack_base - boot_info->lowmem_start);
  for (uint32_t i_virt = 0; i_virt < lowmem_size; i_virt += PAGE_SIZE) {
    vmm_create_page(boot_info->lowmem_start + i_virt, PML_KERNEL_ACCESS);
  }

  vmm_set_directory(k_pdir, k_phy_pdir);
  // vmm_paging(va_dir, pa_dir);
  // log("VMM: Done");
}
