#include <arch/i386/boot/gdt.h>
#include <arch/i386/boot/irq.h>
#include <kernel/kernel.h>

int kenter(void) {
  gdt_init();
  idt_init();

  asm volatile("int $34");

  /* give control to the real kernel main */
  return kmain();
}
