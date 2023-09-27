#include <kernel/kernel.h>
#include <kernel/system/syscall.h>
#include <kernel/process/scheduler.h>
#include <kernel/process/elf.h>
#include <kernel/system/signal.h>
#include <kernel/fs/vfs.h>
#include <kernel/fs/procfs.h>
#include <kernel/drivers/video.h>
#include <kernel/memory/mmu.h>
#include <kernel/printf.h>

/// Initial ESP.
uintptr_t initial_esp = 0;

boot_info_t *boot_info = NULL;

int kinit(boot_info_t *_boot_info) {
  boot_info = _boot_info;

  // Set the initial esp.
  initial_esp = boot_info->stack_base;

  dprintf("Initialize the filesystem...\n");
  vfs_init();

  dprintf("    Initialize 'procfs'...\n");
  if (procfs_module_init()) {
    dprintf("Failed to register `procfs`!\n");
    return 1;
  }

  dprintf("    Mounting 'procfs'...\n");
  if (do_mount("procfs", "/proc", NULL)) {
    dprintf("Failed to mount procfs at `/proc`!\n");
    return 1;
  }

  scheduler_init();

  if (!tasking_init()) {
    dprintf("Init tasking failed!\n");
    return 1;
  }

  if (!signals_init()) {
    dprintf("Init signals failed!\n");
    return 1;
  }

  return 0;
}

int kmain() {
  // char *video_memory = (char *)0xB8000;
  // char *video_memory = (char *)0xC03FF000;
  // *video_memory = 'H';

  // for (int i = 0; i < 80; ++i) {
  //   for (int j = 0; j < 25; ++j) {
  //     video_putc(65);
  //   }
  // }
  char msg[] = "Hello World From Kernel Space!\n";
  video_puts(msg);

  // Create init process
  task_struct *init_p = create_task_test("hello");
  if (!init_p) {
    dprintf("Failed to create task test.\n");
  }

  // We have completed the booting procedure.
  dprintf("Booting done, jumping into userspace process.\n");
  // Switch to the page directory of init.
  vmm_switch_directory(init_p->mm->pgd);
  // Jump into init process.
  scheduler_enter_user_jmp(
    // Entry point.
    init_p->thread.regs.eip,
    // Stack pointer.
    init_p->thread.regs.useresp);

  for (;;) {}

  // We should not be here.
  dprintf("Dear developer, we have to talk...\n");

  return 1;
}
