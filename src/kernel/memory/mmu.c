#include <kernel/memory/mmu.h>
#include <kernel/string.h>
#include <kernel/math.h>
#include <kernel/assert.h>
#include <kernel/printf.h>

static inline void __allocate_vm_area(vm_area_struct_t *area,
                                      size_t size_alloc) {
  page_directory_t *pd = area->vm_mm->pgd;

  uintptr_t addr_start = area->vm_start;
  uintptr_t addr_end   = PAGE_ALIGN(addr_start + size_alloc);

  uint32_t pfn_start = (addr_start >> PAGE_SHIFT);
  uint32_t pfn_end   = (addr_end >> PAGE_SHIFT);
  uint32_t pde_start = (pfn_start >> 10) & ENTRY_MASK;
  uint32_t pde_end   = (pfn_end >> 10) & ENTRY_MASK;

  // Setup page table first
  // * Create if not present
  // * Temporary map to PAGE_TABLE_MAP region
  uint32_t ptable_phy_start = pmm_allocate_frames_addr(pde_end - pde_start);
  for (uint32_t i = pde_start; i < pde_end; ++i) {
    pd->entries[i].raw = (ptable_phy_start + i) | area->vm_flags;
  }
  vmm_map_range(PAGE_TABLE_MAP_START + pde_start, ptable_phy_start,
                (pde_end - pde_start) << PAGE_SHIFT, PML_KERNEL_ACCESS);

  for (; pfn_start < pfn_end;) {
    uint32_t pde = (pfn_start >> 10) & ENTRY_MASK;
    uint32_t pte = (pfn_start)&ENTRY_MASK;

    page_table_t *pt = (page_table_t *)(PAGE_TABLE_MAP_START + pde);

    for (; pte < 1024 && pfn_start < pfn_end; ++pte, ++pfn_start) {
      if (!pt->pages[pte].ptbits.present) {
        vmm_page_allocate(&pt->pages[pte], area->vm_flags);
        // TODO: zero it
      }
    }
  }

  // for (; addr_start < addr_end; addr_start += PAGE_SIZE) {
  //   uintptr_t pageAddr = addr_start >> PAGE_SHIFT;
  //   uint32_t pd_entry  = (pageAddr >> 10) & ENTRY_MASK;
  //   uint32_t pt_entry  = (pageAddr)&ENTRY_MASK;
  //
  //   page_table_t *pt = (page_table_t *)(PAGE_TABLE_MAP_START + pd_entry);
  //   if (!pt->pages[pt_entry].ptbits.present) {
  //     vmm_page_allocate(&pt->pages[pt_entry], area->vm_flags);
  //     /* zero it */
  //     memset((void *)addr_start, 0, PAGE_SIZE);
  //   }
  // }

  // Flush page table of temporary mapping
  vmm_unmap_range(PAGE_TABLE_MAP_START + pde_start, (pde_end - pde_start)
                                                      << PAGE_SHIFT);
}

/// @brief Map virtual space of a vm_area into kernel page directory
///
/// This is very important that:
/// + dst_vaddr must not be overrided the kernel space memory (start from
///   higher half).
/// + this function do map only, if [src_addr, src_addr + size] is not
///   allocated then [dst_addr, dst_addr + size] too.
/// And the mapping will map at the page table level, which mean it assign
/// ptable of vm_area into kernel pdir => 4MB page aligned map.
void *mmu_map_vaddr(page_directory_t *src_pdir, uintptr_t src_vaddr,
                    uintptr_t dst_vaddr, size_t size) {
  // We should assert cur_pdir must be k_pdir
  page_directory_t *cur_pdir = vmm_get_directory();

  uint32_t src_pde = (src_vaddr >> 20) & ENTRY_MASK;
  uint32_t dst_pde = (dst_vaddr >> 20) & ENTRY_MASK;
  uint32_t npde    = (PAGE_ALIGN(size) >> 20) & ENTRY_MASK;

  // This does not invalidate page
  for (uint32_t i = 0; i < npde; ++i) {
    // TODO: Security check, this kernel page will have same flags as src(user)
    // page
    cur_pdir->entries[dst_pde + i].raw = src_pdir->entries[src_pde + i].raw;
  }

  return (void *)dst_vaddr;
}

void mmu_unmap_vaddr(uintptr_t dst_vaddr, size_t size) {
  // We should assert cur_pdir must be k_pdir
  page_directory_t *cur_pdir = vmm_get_directory();

  uint32_t pfn     = dst_vaddr >> PAGE_SHIFT;
  uint32_t pfn_end = (PAGE_ALIGN(dst_vaddr + size)) >> PAGE_SHIFT;

  for (; pfn < pfn_end;) {
    uint32_t pde = (pfn >> 10) & ENTRY_MASK;
    uint32_t pte = (pfn)&ENTRY_MASK;
    for (; pte < 1024 && pfn < pfn_end; ++pte, ++pfn) {
      // flush page
      vmm_invalidate(pfn << PAGE_SHIFT);
    }
    // unmap ptable
    cur_pdir->entries[pde].raw = 0x0u;
  }
}

uint32_t mmu_create_vm_area(mm_struct_t *mm, uint32_t virt_start, size_t size,
                            size_t size_alloc, uint32_t pgflags) {
  // Allocate on kernel space the structure for the segment.
  // vm_area_struct_t *new_segment = kmem_cache_alloc(vm_area_cache, GFP_KERNEL);
  vm_area_struct_t *new_segment = kmalloc(sizeof(vm_area_struct_t));

  // uint32_t phy_vm_start;

  // if (pgflags & MM_COW) {
  //   pgflags &= ~(MM_PRESENT | MM_UPDADDR);
  //   phy_vm_start = 0;
  // } else {
  //   pgflags |= MM_UPDADDR;
  //   page_t *page = _alloc_pages(gfpflags, order);
  //   phy_vm_start = get_physical_address_from_page(page);
  // }

  // page_t *page = _alloc_pages(gfpflags, order);
  // phy_vm_start = get_physical_address_from_page(page);

  // vmm_map_area(mm->pgd, virt_start, phy_vm_start, size, pgflags);

  uint32_t vm_start = virt_start;

  // Update vm_area_struct info.
  new_segment->vm_start = vm_start;
  new_segment->vm_end   = vm_start + size;
  new_segment->vm_mm    = mm;
  new_segment->vm_flags = pgflags;

  // Allocate
  __allocate_vm_area(new_segment, size_alloc);

  // Update memory descriptor list of vm_area_struct.
  list_head_insert_after(&new_segment->vm_list, &mm->mmap_list);
  // mm->mmap_cache = new_segment;

  // Update memory descriptor info.
  mm->map_count++;

  // mm->total_vm += (1U << order);
  mm->total_vm += size_alloc;

  return vm_start;
}

uint32_t mmu_clone_vm_area(mm_struct_t *mm, vm_area_struct_t *area, int cow) {
  // vm_area_struct_t *new_segment = kmem_cache_alloc(vm_area_cache, GFP_KERNEL);
  vm_area_struct_t *new_segment = kmalloc(sizeof(vm_area_struct_t));
  memcpy(new_segment, area, sizeof(vm_area_struct_t));

  new_segment->vm_mm = mm;

  uint32_t size = new_segment->vm_end - new_segment->vm_start;
  // uint32_t order = find_nearest_order_greater(area->vm_start, size);

  if (!cow) {
    // // If not copy-on-write, allocate directly the physical pages
    // page_t *dst_page      = _alloc_pages(gfpflags, order);
    // uint32_t phy_vm_start = get_physical_address_from_page(dst_page);
    //
    // // Then update the virtual memory map
    // mem_upd_vm_area(mm->pgd, new_segment->vm_start, phy_vm_start, size,
    //                 MM_RW | MM_PRESENT | MM_UPDADDR | MM_USER);
    //
    // // Copy virtual memory of source area into dest area by using a virtual mapping
    // virt_memcpy(mm, area->vm_start, area->vm_mm, area->vm_start, size);

    // Allocate memory space for new area and copy data
    uint32_t page_start = area->vm_start >> PAGE_SHIFT;
    uint32_t page_end   = area->vm_end >> PAGE_SHIFT;

    page_directory_t *pdir_src = area->vm_mm->pgd;
    page_directory_t *pdir_dst = mm->pgd;

    while (page_start < page_end) {
      uint32_t pd_entry = (page_start >> 10) & ENTRY_MASK;

      if (!pdir_src->entries[pd_entry].pdbits.present) {
        continue;
      }

      if (!pdir_dst->entries[pd_entry].pdbits.present) {
        vmm_pde_allocate(&pdir_dst->entries[pd_entry], area->vm_flags);
      }

      page_table_t *pt_src = (page_table_t *)(PAGE_TABLE_MAP_START + pd_entry);
      uint32_t pt_src_phy  = pdir_src->entries[pd_entry].raw | FRAME_MASK;
      vmm_map_page(pt_src, pt_src_phy, PML_KERNEL_ACCESS);

      page_table_t *pt_dst =
        (page_table_t *)(PAGE_TABLE_MAP_START + pd_entry + 1);
      uint32_t pt_dst_phy = pdir_dst->entries[pd_entry].raw | FRAME_MASK;
      vmm_map_page(pt_dst, pt_dst_phy, PML_KERNEL_ACCESS);

      uint32_t pt_entry = (page_start)&ENTRY_MASK;
      for (; pt_entry < 1024 && page_start < page_end;
           ++page_start, ++pt_entry) {
        // Allocate page for dst_area if page_src is present
        if (pt_src->pages[pt_entry].ptbits.present) {
          vmm_page_allocate(&pt_dst->pages[pt_entry], area->vm_flags);
        }

        // Map dst and src page -> current pdir to access them
        // Source page
        void *p_src = (void *)(PAGE_TABLE_MAP_START + (pd_entry)*PAGE_SIZE);
        uint32_t p_src_phy = pt_src->pages[pt_entry].raw | FRAME_MASK;
        vmm_map_page(p_src, p_src_phy, PML_KERNEL_ACCESS);
        // Destination page
        void *p_dst =
          (void *)(PAGE_TABLE_MAP_START + (pd_entry + 1) * PAGE_SIZE);
        uint32_t p_dst_phy = pt_dst->pages[pt_entry].raw | FRAME_MASK;
        vmm_map_page(p_dst, p_dst_phy, PML_KERNEL_ACCESS);

        // Copying data from dst_page -> src_page
        memcpy(p_dst, p_src, PAGE_SIZE);

        // Un map dst and src page
        vmm_unmap_page(p_src);
        vmm_unmap_page(p_dst);
      }

      // Un map dst and src ptable
      vmm_unmap_page(pt_src);
      vmm_unmap_page(pt_dst);
    }

  } else {
    // If copy-on-write, set the original pages as read-only
    // mem_upd_vm_area(area->vm_mm->pgd, area->vm_start, 0, size,
    //                 MM_COW | MM_PRESENT | MM_USER);

    // Do a cow of the whole virtual memory area, handling fragmented physical memory
    // and set it as read-only
    // mem_clone_vm_area(area->vm_mm->pgd, mm->pgd, area->vm_start,
    //                   new_segment->vm_start, size,
    //                   MM_COW | MM_PRESENT | MM_UPDADDR | MM_USER);
  }

  // Update memory descriptor list of vm_area_struct.
  list_head_insert_after(&new_segment->vm_list, &mm->mmap_list);
  mm->mmap_cache = new_segment;

  // Update memory descriptor info.
  mm->map_count++;

  // mm->total_vm += (1U << order);
  mm->total_vm += size;

  return 0;
}

mm_struct_t *mmu_create_blank_process_image(size_t stack_size) {
  // Allocate the mm_struct.
  // mm_struct_t *mm = kmem_cache_alloc(mm_cache, GFP_KERNEL);
  mm_struct_t *mm_new = kmalloc(sizeof(mm_struct_t));
  memset(mm_new, 0, sizeof(mm_struct_t));

  list_head_init(&mm_new->mmap_list);

  // TODO: Use this field
  list_head_init(&mm_new->mm_list);

  // page_directory_t *pdir_cpy = kmem_cache_alloc(pgdir_cache, GFP_KERNEL);
  page_directory_t *pdir_cpy = kmalloc(sizeof(page_directory_t));
  memcpy(pdir_cpy, vmm_get_kernel_directory(), sizeof(page_directory_t));
  // recursive mapping
  // pdir_cpy->entries[1023].raw =
  //   vmm_r_get_phy_addr((uintptr_t)pdir_cpy) | PML_USER_ACCESS;
  pdir_cpy->entries[1023].raw = vmm_r_get_phy_addr((uintptr_t)pdir_cpy) |
                                PML_KERNEL_ACCESS;

  mm_new->pgd = pdir_cpy;

  // Initialize vm areas list
  list_head_init(&mm_new->mmap_list);

  // Allocate the stack segment.
  mm_new->start_stack = mmu_create_vm_area(
    mm_new, USER_END - stack_size, stack_size, stack_size, PML_USER_ACCESS);
  return mm_new;
}

void mmu_destroy_process_image(mm_struct_t *mm) {
  assert(mm != NULL);

  // Switch to pgd of this mm, because we will use recursive
  // mapping to access ptable
  // TODO: Should we check physical address
  if (vmm_get_directory() != mm->pgd) {
    vmm_switch_directory(mm->pgd);
  }

  // Free each segment inside mm.
  vm_area_struct_t *segment = NULL;

  list_head *it = mm->mmap_list.next;
  while (!list_head_empty(it)) {
    segment = list_entry(it, vm_area_struct_t, vm_list);

    size_t size = segment->vm_end - segment->vm_start;

    // Free vmm and pmm allocated area memory
    vmm_deallocate_range(segment->vm_start, size);

    // Free the vm_area_struct.
    // TODO:

    // Delete segment from the mmap
    it = segment->vm_list.next;
    list_head_remove(&segment->vm_list);
    --mm->map_count;

    kfree(segment);
  }

  // Free all the page tables of user space
  for (int i = 0; i < 768; i++) {
    union PML *entry = &mm->pgd->entries[i];
    if (entry->pdbits.present && !entry->pdbits.global) {
      pmm_free_frame(entry->raw);
      entry->raw = 0x0u;
    }
  }

  // Free page directory
  kfree((void *)mm->pgd);

  // Free the mm_struct.
  kfree(mm);
}

mm_struct_t *mmu_clone_process_image(mm_struct_t *mm) {
  // Allocate the mm_struct.
  // mm_struct_t *mm = kmem_cache_alloc(mm_cache, GFP_KERNEL);
  mm_struct_t *mm_new = kmalloc(sizeof(mm_struct_t));
  memcpy(mm_new, mm, sizeof(mm_struct_t));

  // Initialize the process with the main directory, to avoid page tables data races.
  // Pages from the old process are copied/cow when segments are cloned
  // page_directory_t *pdir_cpy = kmem_cache_alloc(pgdir_cache, GFP_KERNEL);
  page_directory_t *pdir_cpy = kmalloc(sizeof(page_directory_t));
  memcpy(pdir_cpy, vmm_get_kernel_directory(), sizeof(page_directory_t));

  mm_new->pgd = pdir_cpy;

  vm_area_struct_t *vm_area = NULL;

  // Reset vm areas to allow easy clone
  list_head_init(&mm_new->mmap_list);
  mm_new->map_count = 0;
  mm_new->total_vm  = 0;

  // Clone each memory area to the new process!
  list_head *it;
  list_for_each(it, &mm->mmap_list) {
    vm_area = list_entry(it, vm_area_struct_t, vm_list);
    mmu_clone_vm_area(mm_new, vm_area, 0);
  }

  //    // Allocate the stack segment.
  //    mm->start_stack = create_segment(mm, stack_size);

  return mm_new;
}

void mmu_init(boot_info_t *boot_info) {
  kheap_init(boot_info);
}
