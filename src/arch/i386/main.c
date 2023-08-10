#include <arch/i386/gdt.h>
#include <arch/i386/irq.h>
#include <arch/i386/pic.h>
#include <arch/i386/pit.h>
#include <arch/i386/cpu.h>
#include <kernel/kernel.h>
#include <kernel/multiboot.h>
#include <kernel/memory/pmm.h>
#include <kernel/drivers/vga.h>

int kenter(uint32_t magic, uint32_t addr) {
  multiboot_init(magic, addr);

  gdt_init();
  idt_init();
  pmm_init(mboot.multiboot_meminfo, mboot.multiboot_mmap);
  pic_init();
  pit_init();
  vga_init();

  enable_interrupts();

  /* give control to the real kernel main */
  return kmain();
}
