#include <arch/i386/gdt.h>
#include <arch/i386/irq.h>
#include <arch/i386/pic.h>
#include <arch/i386/pit.h>
#include <arch/i386/cpu.h>
#include <kernel/kernel.h>
#include <kernel/boot.h>
#include <kernel/multiboot.h>
#include <kernel/drivers/vga.h>
#include <kernel/memory/pmm.h>
#include <kernel/memory/vmm.h>
#include <kernel/memory/mmu.h>

#include <kernel/printf.h>

/// @brief Boot info provided to the kmain function.
static struct boot_info_t boot_info;

int kenter(uint32_t magic, uint32_t addr) {
  vga_init(&boot_info);
  // Init multiboot structure
  multiboot_init(magic, addr);

  boot_info.magic = magic;
  boot_info.multiboot_header = &mboot;

  // Get the physical addresses of where the kernel starts and ends.
  boot_info.bootloader_phy_start = BOOTLOADER_START;
  boot_info.bootloader_phy_end = BOOTLOADER_END;

  // Get the addresses of kernel
  boot_info.kernel_phy_start = KERNEL_START - KERNEL_HIGHER_HALF;
  boot_info.kernel_phy_end = KERNEL_END - KERNEL_HIGHER_HALF;
  boot_info.kernel_start = KERNEL_START;
  boot_info.kernel_end = KERNEL_END;
  boot_info.kernel_size = KERNEL_START - KERNEL_END;

  // Get the maximum memory
  uint32_t highest_addr =
    get_highest_valid_address(mboot.multiboot_meminfo, mboot.multiboot_mmap);

  // Get the addresses of lowmem and highmem zones.
  boot_info.lowmem_phy_start = __ALIGN_UP(boot_info.kernel_phy_end, PAGE_SIZE);
  boot_info.lowmem_phy_end = 896 * MB; // 896 MB of low memory max
  if (boot_info.lowmem_phy_end > highest_addr) {
    dprintf("Low lowmem region:\n"
            " available: 0x%p, size: %uGB %uMB %uKB\n"
            " desired: 0x%p, size: %uGB %uMB %uKB\n",
            highest_addr, highest_addr / GB, highest_addr / MB,
            highest_addr / KB, boot_info.lowmem_phy_end,
            boot_info.lowmem_phy_end / GB, boot_info.lowmem_phy_end / MB,
            boot_info.lowmem_phy_end / KB);
    dprintf("-> Zero space highmem region!\n");
    boot_info.lowmem_phy_end = highest_addr;
  }
  uint32_t lowmem_size = boot_info.lowmem_phy_end - boot_info.lowmem_phy_start;

  boot_info.lowmem_start = __ALIGN_UP(boot_info.kernel_end, PAGE_SIZE);
  boot_info.lowmem_end = boot_info.lowmem_start + lowmem_size;

  boot_info.highmem_phy_start = boot_info.lowmem_phy_end;
  boot_info.highmem_phy_end = highest_addr;
  boot_info.stack_end = boot_info.lowmem_end;

  // Reserve space for the kernel stack at the end of lowmem.
  boot_info.stack_base = KERNEL_STACK_TOP;
  boot_info.lowmem_phy_end = boot_info.lowmem_phy_end - KERNEL_STACK_SIZE;
  boot_info.lowmem_end = boot_info.lowmem_end - KERNEL_STACK_SIZE;
  boot_info.lowmem_current = boot_info.lowmem_start;

  // Reserve space for kernel heap
  boot_info.heap_start = KERNEL_HEAP_START;
  boot_info.heap_end = KERNEL_HEAP_END;
  uint32_t heap_size = boot_info.heap_end - boot_info.heap_start;
  // 128 KB reserve for frame bit map for 32 bit address space
  uint32_t lowmem_reserved = 128 * KB;
  if (heap_size > (lowmem_size - lowmem_reserved)) {
    dprintf("Low kernel heap memory:\n"
            " available: %uKB\n"
            " desired: %uKB\n",
            __ALIGN_DOWN(lowmem_size - lowmem_reserved, PAGE_SIZE) / KB,
            heap_size / KB);
    boot_info.heap_end = boot_info.heap_start +
                         __ALIGN_DOWN(lowmem_size - lowmem_reserved, PAGE_SIZE);
    heap_size = boot_info.heap_end - boot_info.heap_start;
  }
  dprintf("Memory summary: \n"
          " total: %uGB + %uMB + %uKB\n"
          " lowmem: phy=0x%p virt=0x%p (%uKB)\n"
          " heap: virt=0x%p (%uKB)\n",
          highest_addr / GB, (highest_addr % GB) / MB,
          ((highest_addr % GB) % MB) / KB, boot_info.lowmem_phy_start,
          boot_info.lowmem_start, lowmem_size / KB, boot_info.heap_start,
          heap_size / KB);

  // Initialization
  gdt_init();
  idt_init();
  pmm_init(&boot_info);
  vmm_init(&boot_info);
  mmu_init(&boot_info);
  pic_init();
  pit_init();

  enable_interrupts();

  /* give control to the real kernel main */
  return kmain();
}
