#include <kernel/memory/mmu.h>
#include <kernel/string.h>
#include <kernel/assert.h>

void mmu_init(boot_info_t *boot_info) {
  kheap_init(boot_info);
}

uint32_t create_vm_area(mm_struct_t *mm, uint32_t virt_start, size_t size,
                        uint32_t pgflags) {
  // Allocate on kernel space the structure for the segment.
  // vm_area_struct_t *new_segment = kmem_cache_alloc(vm_area_cache, GFP_KERNEL);
  vm_area_struct_t *new_segment = kmalloc(sizeof(vm_area_struct_t));

  uint32_t order = find_nearest_order_greater(virt_start, size);

  uint32_t phy_vm_start;

  // if (pgflags & MM_COW) {
  //   pgflags &= ~(MM_PRESENT | MM_UPDADDR);
  //   phy_vm_start = 0;
  // } else {
  //   pgflags |= MM_UPDADDR;
  //   page_t *page = _alloc_pages(gfpflags, order);
  //   phy_vm_start = get_physical_address_from_page(page);
  // }
  
    page_t *page = _alloc_pages(gfpflags, order);
    phy_vm_start = get_physical_address_from_page(page);

  vmm_map_area(mm->pgd, virt_start, phy_vm_start, size, pgflags);

  uint32_t vm_start = virt_start;

  // Update vm_area_struct info.
  new_segment->vm_start = vm_start;
  new_segment->vm_end = vm_start + size;
  new_segment->vm_mm = mm;

  // Update memory descriptor list of vm_area_struct.
  list_head_insert_after(&new_segment->vm_list, &mm->mmap_list);
  mm->mmap_cache = new_segment;

  // Update memory descriptor info.
  mm->map_count++;

  mm->total_vm += (1U << order);

  return vm_start;
}

uint32_t mmu_clone_vm_area(mm_struct_t *mm, vm_area_struct_t *area, int cow) {
  // vm_area_struct_t *new_segment = kmem_cache_alloc(vm_area_cache, GFP_KERNEL);
  vm_area_struct_t *new_segment = kmalloc(vm_area_cache, GFP_KERNEL);
  memcpy(new_segment, area, sizeof(vm_area_struct_t));

  new_segment->vm_mm = mm;

  uint32_t size = new_segment->vm_end - new_segment->vm_start;
  uint32_t order = find_nearest_order_greater(area->vm_start, size);

  if (!cow) {
    // If not copy-on-write, allocate directly the physical pages
    page_t *dst_page = _alloc_pages(gfpflags, order);
    uint32_t phy_vm_start = get_physical_address_from_page(dst_page);

    // Then update the virtual memory map
    mem_upd_vm_area(mm->pgd, new_segment->vm_start, phy_vm_start, size,
                    MM_RW | MM_PRESENT | MM_UPDADDR | MM_USER);

    // Copy virtual memory of source area into dest area by using a virtual mapping
    virt_memcpy(mm, area->vm_start, area->vm_mm, area->vm_start, size);
  } else {
    // If copy-on-write, set the original pages as read-only
    mem_upd_vm_area(area->vm_mm->pgd, area->vm_start, 0, size,
                    MM_COW | MM_PRESENT | MM_USER);

    // Do a cow of the whole virtual memory area, handling fragmented physical memory
    // and set it as read-only
    mem_clone_vm_area(area->vm_mm->pgd, mm->pgd, area->vm_start,
                      new_segment->vm_start, size,
                      MM_COW | MM_PRESENT | MM_UPDADDR | MM_USER);
  }

  // Update memory descriptor list of vm_area_struct.
  list_head_insert_after(&new_segment->vm_list, &mm->mmap_list);
  mm->mmap_cache = new_segment;

  // Update memory descriptor info.
  mm->map_count++;

  mm->total_vm += (1U << order);

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
  pdir_cpy->entries[1023].raw = vmm_r_get_phy_addr((uintptr_t)pdir_cpy);

  mm_new->pgd = pdir_cpy;

  // Initialize vm areas list
  list_head_init(&mm_new->mmap_list);

  // Allocate the stack segment.
  mm_new->start_stack = mmu_create_vm_area(mm_new, USER_END - stack_size,
                                           stack_size, PML_USER_ACCESS);
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
    vmm_deallocate_area(segment->vm_start, size);

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

mm_struct_t *clone_process_image(mm_struct_t *mm) {
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
  mm_new->total_vm = 0;

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
