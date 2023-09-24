#include <arch/i386/pic.h>
#include <arch/i386/cpu.h>
#include <arch/i386/ports.h>

void pic_remap() {
  unsigned char a1, a2;

  a1 = inportb(PIC1_DATA); // save masks
  a2 = inportb(PIC2_DATA);

  // starts the initialization sequence (in cascade mode)
  outportb(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4);
  io_wait();
  outportb(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);
  io_wait();

  // ICW2: Master PIC vector offset
  outportb(PIC1_DATA, 0x20);
  io_wait();
  // ICW2: Slave PIC vector offset
  outportb(PIC2_DATA, 0x28);
  io_wait();

  // ICW3: tell Master PIC that there is a slave PIC at IRQ2 (0000 0100)
  outportb(PIC1_DATA, 4);
  io_wait();
  // ICW3: tell Slave PIC its cascade identity (0000 0010)
  outportb(PIC2_DATA, 2);
  io_wait();

  outportb(PIC1_DATA, ICW4_8086);
  io_wait();
  outportb(PIC2_DATA, ICW4_8086);
  io_wait();

  outportb(PIC1_DATA, a1); // restore saved masks.
  outportb(PIC2_DATA, a2);
}

void pic_set_mask(unsigned char irq_line) {
  uint16_t port;
  uint8_t value;

  if (irq_line < 8) {
    port = PIC1_DATA;
  } else {
    port = PIC2_DATA;
    irq_line -= 8;
  }
  value = inportb(port) | (1 << irq_line);
  outportb(port, value);
}

void pic_clear_mask(unsigned char irq_line) {
  uint16_t port;
  uint8_t value;

  if (irq_line < 8) {
    port = PIC1_DATA;
  } else {
    port = PIC2_DATA;
    irq_line -= 8;
  }
  value = inportb(port) & ~(1 << irq_line);
  outportb(port, value);
}

void pic_init(void) {
  pic_remap();
}
