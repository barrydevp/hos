#include <arch/i386/gdt.h>
#include <arch/i386/irq.h>
#include <arch/i386/pic.h>
#include <arch/i386/pit.h>
#include <arch/i386/cpu.h>
#include <kernel/kernel.h>

int kenter(void) {
  gdt_init();
  idt_init();
  pic_init();
  pit_init();

  enable_interrupts();

  /* give control to the real kernel main */
  return kmain();
}
