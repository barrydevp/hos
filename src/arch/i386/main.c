#include <arch/i386/gdt.h>
#include <arch/i386/irq.h>
#include <arch/i386/pic.h>
#include <arch/i386/pit.h>
#include <arch/i386/cpu.h>
#include <kernel/kernel.h>
#include <kernel/boot.h>
#include <kernel/multiboot.h>
#include <kernel/memory/pmm.h>
#include <kernel/memory/vmm.h>
#include <kernel/drivers/vga.h>

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

  // Get the addresses of lowmem and highmem zones.
  boot_info.lowmem_phy_start = __ALIGN_UP(boot_info.kernel_phy_end, PAGE_SIZE);
  boot_info.lowmem_phy_end = 896 * 1024 * 1024; // 896 MB of low memory max
  uint32_t lowmem_size = boot_info.lowmem_phy_end - boot_info.lowmem_phy_start;

  boot_info.lowmem_start = __ALIGN_UP(boot_info.kernel_end, PAGE_SIZE);
  boot_info.lowmem_end = boot_info.lowmem_start + lowmem_size;

  boot_info.highmem_phy_start = boot_info.lowmem_phy_end;
  boot_info.highmem_phy_end = mboot.multiboot_meminfo->mem_upper * 1024;
  boot_info.stack_end = boot_info.lowmem_end;

  // Reserve space for the kernel stack at the end of lowmem.
  boot_info.stack_base = boot_info.lowmem_end;
  boot_info.lowmem_phy_end = boot_info.lowmem_phy_end - KERNEL_STACK_SIZE;
  boot_info.lowmem_end = boot_info.lowmem_end - KERNEL_STACK_SIZE;
  boot_info.lowmem_current = boot_info.lowmem_start;

  // Reserve space for kernel heap
  boot_info.heap_start = KERNEL_HEAP_START;
  boot_info.heap_end = KERNEL_HEAP_END;
  if (boot_info.lowmem_end < KERNEL_HEAP_END) {
    boot_info.heap_end = boot_info.lowmem_end;
  }

  // Initialization
  gdt_init();
  idt_init();
  pmm_init(&boot_info);
  vmm_init(&boot_info);
  pic_init();
  pit_init();

  enable_interrupts();

  /* give control to the real kernel main */
  return kmain();
}
