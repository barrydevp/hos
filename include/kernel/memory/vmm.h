/*
 * Resources: https://wiki.osdev.org/Page_Tables
 * https://github.com/MQuy/mos/blob/8327ee4f139f01540e23d71245821cf26a173ef6/src/kernel/memory/vmm.h
 * https://github.com/klange/toaruos/blob/e24cdc168170d0fa4c49d150376c09a6924fae23/base/usr/include/kernel/mmu.h
 */
#pragma once

#include <kernel/types.h>
#include <kernel/kernel.h>
#include <kernel/boot.h>
#include <kernel/list_head.h>
#include <kernel/memory/pmm.h>

#define PAGE_SIZE 4096
#define PAGE_SHIFT 12
#define PAGE_MASK (~(PAGE_SIZE - 1))
#define PAGE_LOW_MASK (PAGE_SIZE - 1)
#define PAGE_ALIGN(addr) (((addr) + PAGE_LOW_MASK) & PAGE_MASK)
#define PAGE_TABLE_SIZE 1024
#define ENTRY_MASK 0x3FF // 10 bits
//! i86 architecture defines 1024 entries per table--do not change
#define PAGES_PER_TABLE 1024
#define PAGES_PER_DIR 1024
// Base address of last entry (1024) recursive mapping
#define PAGE_DIRECTORY_BASE 0xFFFF0000
#define PAGE_TABLE_BASE 0xFFC00000

#define PDE_INDEX(x) (((x) >> 22) & ENTRY_MASK)
#define PTE_INDEX(x) (((x) >> 12) & ENTRY_MASK)
// PML = Page Multiple Level , we use this name alias for both PDE and PTE
// since some first bits of them have the same meaning
#define PML_KERNEL_ACCESS 0x03
#define PML_USER_ACCESS 0x07
#define LARGE_PAGE_BIT 0x08
#define PML_DIR_VADDR 0xFFFF0000
#define PML_COW_BIT 0x200

#define VMM_FLAG_KERNEL 0x01
#define VMM_FLAG_WRITABLE 0x02
#define VMM_FLAG_NOCACHE 0x04
#define VMM_FLAG_WRITETHROUGH 0x08
// #define VMM_FLAG_SPEC         0x10
// #define VMM_FLAG_WC           (MMU_FLAG_NOCACHE | MMU_FLAG_WRITETHROUGH | MMU_FLAG_SPEC)
// #define VMM_FLAG_NOEXECUTE    0x20

#define VMM_GET_MAKE 0x01

#define VMM_PTR_NULL 1
#define VMM_PTR_WRITE 2

union PML {
  struct {
    uint32_t present : 1;
    uint32_t writable : 1;
    uint32_t user : 1;
    uint32_t writethrough : 1;
    uint32_t nocache : 1;
    uint32_t accessed : 1;
    uint32_t reserved : 1;
    uint32_t size : 1;
    uint32_t global : 1;
    uint32_t _available : 3;
    uint32_t frame : 20;
  } pdbits;
  struct {
    uint32_t present : 1;
    uint32_t writable : 1;
    uint32_t user : 1;
    uint32_t reserved1 : 2;
    uint32_t accessed : 1;
    uint32_t dirty : 1;
    uint32_t reserved2 : 2;
    uint32_t cow : 1;
    uint32_t _available : 2;
    uint32_t frame : 20;
  } ptbits;
  uint32_t raw;
};

//! this format is defined by the i86 architecture--be careful if you modify it
enum PAGE_PDE_FLAGS {
  I86_PDE_PRESENT = 0x1, //0000000000000000000000000000001
  I86_PDE_WRITABLE = 0x2, //0000000000000000000000000000010
  I86_PDE_USER = 0x4, //0000000000000000000000000000100
  I86_PDE_PWT = 0x8, //0000000000000000000000000001000
  I86_PDE_PCD = 0x10, //0000000000000000000000000010000
  I86_PDE_ACCESSED = 0x20, //0000000000000000000000000100000
  I86_PDE_DIRTY = 0x40, //0000000000000000000000001000000
  I86_PDE_4MB = 0x80, //0000000000000000000000010000000
  I86_PDE_CPU_GLOBAL = 0x100, //0000000000000000000000100000000
  I86_PDE_LV4_GLOBAL = 0x200, //0000000000000000000001000000000
  I86_PDE_FRAME = 0x7FFFF000 //1111111111111111111000000000000
};

//! i86 architecture defines this format so be careful if you modify it
enum PAGE_PTE_FLAGS {
  I86_PTE_PRESENT = 0x1, //0000000000000000000000000000001
  I86_PTE_WRITABLE = 0x2, //0000000000000000000000000000010
  I86_PTE_USER = 0x4, //0000000000000000000000000000100
  I86_PTE_WRITETHOUGH = 0x8, //0000000000000000000000000001000
  I86_PTE_NOT_CACHEABLE = 0x10, //0000000000000000000000000010000
  I86_PTE_ACCESSED = 0x20, //0000000000000000000000000100000
  I86_PTE_DIRTY = 0x40, //0000000000000000000000001000000
  I86_PTE_PAT = 0x80, //0000000000000000000000010000000
  I86_PTE_CPU_GLOBAL = 0x100, //0000000000000000000000100000000
  I86_PTE_LV4_GLOBAL = 0x200, //0000000000000000000001000000000
  I86_PTE_FRAME = 0x7FFFF000 //1111111111111111111000000000000
};

/// @brief A page table.
/// @details
/// It contains 1024 entries which can be addressed by 10 bits (log_2(1024)).
typedef struct page_table_t {
  union PML pages[1024]; ///< Array of pages.
} __attribute__((aligned(PAGE_SIZE))) page_table_t;

/// @brief A page directory.
/// @details In the two-level paging, this is the first level.
typedef struct page_directory_t {
  /// We need a table that contains virtual address, so that we can
  /// actually get to the tables (size: 1024 * 4 = 4096 byte).
  union PML entries[1024];
} __attribute__((aligned(PAGE_SIZE))) page_directory_t;

/// @brief Virtual Memory Area, used to store details of a process segment.
typedef struct vm_area_struct_t {
  /// Memory descriptor associated.
  struct mm_struct_t *vm_mm;
  /// Start address of the segment, inclusive.
  uint32_t vm_start;
  /// End address of the segment, exclusive.
  uint32_t vm_end;
  /// List of memory areas.
  list_head vm_list;
  /// Permissions.
  pgprot_t vm_page_prot;
  /// Flags.
  uint32_t vm_flags;
  /// rbtree node.
  // struct rb_node vm_rb;
} vm_area_struct_t;

/// @brief Memory Descriptor, used to store details about the memory of a user process.
typedef struct mm_struct_t {
  /// List of memory area (vm_area_struct reference).
  list_head mmap_list;
  // /// rbtree of memory area.
  // struct rb_root mm_rb;
  /// Last memory area used.
  vm_area_struct_t *mmap_cache;
  /// Process page directory.
  page_directory_t *pgd;
  /// Number of memory area.
  int map_count;
  /// List of mm_struct.
  list_head mm_list;
  /// CODE start.
  uint32_t start_code;
  /// CODE end.
  uint32_t end_code;
  /// DATA start.
  uint32_t start_data;
  /// DATA end.
  uint32_t end_data;
  /// HEAP start.
  uint32_t start_brk;
  /// HEAP end.
  uint32_t brk;
  /// STACK start.
  uint32_t start_stack;
  /// ARGS start.
  uint32_t arg_start;
  /// ARGS end.
  uint32_t arg_end;
  /// ENVIRONMENT start.
  uint32_t env_start;
  /// ENVIRONMENT end.
  uint32_t env_end;
  /// Number of mapped pages.
  unsigned int total_vm;
} mm_struct_t;

void vmm_page_allocate(union PML *page, uint32_t flags);
void vmm_page_map_addr(union PML *page, uint32_t flags, uintptr_t physAddr);

void vmm_pde_allocate(union PML *pde, uint32_t flags);

union PML *vmm_create_page(uintptr_t virtAddr, uint32_t flags);
union PML *vmm_get_page(uintptr_t virtAddr);
union PML *vmm_map_page(uintptr_t virtAddr, uintptr_t physAddr, uint32_t flags);
void vmm_unmap_page(uintptr_t virtAddr);
uintptr_t vmm_r_get_phy_addr(uintptr_t virtAddr);

void vmm_map_range(uintptr_t virtAddr, uintptr_t physAddr, uint32_t size,
                  uint32_t flags);
void vmm_unmap_range(uintptr_t virtAddr, uint32_t size);
void vmm_allocate_range(uintptr_t virtAddr, uint32_t size, uint32_t flags);
void vmm_deallocate_range(uintptr_t virtAddr, uint32_t size);

page_directory_t *vmm_get_kernel_directory(void);
page_directory_t *vmm_get_directory(void);
void vmm_switch_directory(page_directory_t *new_pdir);
page_directory_t *vmm_clone_pdir(page_directory_t *from);

void vmm_invalidate(uintptr_t addr);
void paging_flush_tlb_single(uintptr_t addr);

void vmm_init(struct boot_info_t *boot_info);
