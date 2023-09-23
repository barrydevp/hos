#include <kernel/drivers/video.h>

/* This will force us to create a kernel entry function instead of jumping to
 * kernel.c:0x00 */
void dummy_test_entrypoint() {
}

int kmain() {
  // char *video_memory = (char *)0xB8000;
  // char *video_memory = (char *)0xC03FF000;
  // *video_memory = 'H';

  for (int i = 0; i < 80; ++i) {
    for (int j = 0; j < 25; ++j) {
      video_putc(65);
    }
  }
  char msg[] = "Hello World!";
  video_puts(msg);

  return 0;
}
