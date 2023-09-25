#include <kernel/kernel.h>
#include <kernel/process/scheduler.h>
#include <kernel/drivers/video.h>

/// Initial ESP.
uintptr_t initial_esp = 0;

boot_info_t *boot_info = NULL;

int kinit(boot_info_t *_boot_info) {
  boot_info = _boot_info;

  // Set the initial esp.
  initial_esp = boot_info->stack_base;

  scheduler_init();

  return kmain();
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
