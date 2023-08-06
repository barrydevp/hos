#include <kernel/drivers/vga.h>

/* This will force us to create a kernel entry function instead of jumping to
 * kernel.c:0x00 */
void dummy_test_entrypoint() {
}

int kmain() {
  // char *video_memory = (char *)0xB8000;
  // char *video_memory = (char *)0xC03FF000;
  // *video_memory = 'H';

  char msg[] = "Hello World!";
  vga_puts(msg);

  for (;;) {
  }

  return 0;
}
