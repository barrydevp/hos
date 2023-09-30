#include <arch/i386/fpu.h>

#include <kernel/kernel.h>
#include <kernel/process/scheduler.h>
#include <kernel/process/elf.h>
#include <kernel/system/signal.h>
#include <kernel/system/syscall.h>
#include <kernel/memory/mmu.h>

#include <kernel/drivers/video.h>
#include <kernel/drivers/ps2.h>
#include <kernel/drivers/keyboard.h>
#include <kernel/drivers/keyboard/keymap.h>

#include <kernel/fs/vfs.h>
#include <kernel/fs/procfs.h>
#include <kernel/fs/proc_modules.h>
#include "kernel/fs/ata.h"
#include "kernel/fs/ext2.h"

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

  dprintf("Initialize ATA devices...\n");
  if (ata_init()) {
    dprintf("Failed to initialize ATA devices!\n");
    return 1;
  }

  dprintf("Initialize EXT2 filesystem...\n");
  if (ext2_init()) {
    dprintf("Failed to initialize EXT2 filesystem!\n");
    return 1;
  }

  dprintf("Mount EXT2 filesystem...\n");
  if (do_mount("ext2", "/", "/dev/hda")) {
    dprintf("Failed to mount EXT2 filesystem...\n");
    return 1;
  }

  dprintf("Initialize 'procfs'...\n");
  if (procfs_module_init()) {
    dprintf("Failed to register `procfs`!\n");
    return 1;
  }

  dprintf("Mounting 'procfs'...\n");
  if (do_mount("procfs", "/proc", NULL)) {
    dprintf("Failed to mount procfs at `/proc`!\n");
    return 1;
  }

  dprintf("Initialize video procfs file...\n");
  if (procv_module_init()) {
    dprintf("Failed to initialize `/proc/video`!\n");
    return 1;
  }

  dprintf("Initialize system procfs file...\n");
  if (procs_module_init()) {
    dprintf("Failed to initialize proc system entries!\n");
    return 1;
  }

  dprintf("Setting up PS/2 driver...\n");
  if (ps2_init()) {
    dprintf("Failed to initialize proc system entries!\n");
    return 1;
  }

  dprintf("Setting up keyboard driver...\n");
  keyboard_init();
  dprintf("Set the keyboard layout: 'US'\n");
  set_keymap_type(KEYMAP_US);

  dprintf("Initialize the scheduler.\n");
  scheduler_init();

  if (!tasking_init()) {
    dprintf("Init tasking failed!\n");
    return 1;
  }

  dprintf("Creating init process...\n");
  task_struct *init_p = process_create_init("/bin/init");
  if (!init_p) {
    dprintf("Create init process failed!\n");
    return 1;
  }

  // dprintf("Initialize floating point unit...\n");
  // if (!fpu_init()) {
  //   dprintf("Init floating point uint failed!\n");
  //   return 1;
  // }

  dprintf("Initialize signals...\n");
  if (!signals_init()) {
    dprintf("Init signals failed!\n");
    return 1;
  }

  return 0;
}

int kmain() {
  char msg[] = "Hello World From Kernel Space!\n";
  video_puts(msg);

  // // Create init process
  // task_struct *init_p = create_task_test("hello");
  // if (!init_p) {
  //   dprintf("Failed to create task test.\n");
  // }
  //
  // // We have completed the booting procedure.
  // dprintf("Booting done, jumping into userspace process.\n");
  // // Switch to the page directory of init.
  // vmm_switch_directory(init_p->mm->pgd);
  // // Jump into init process.
  // scheduler_enter_user_jmp(
  //   // Entry point.
  //   init_p->thread.regs.eip,
  //   // Stack pointer.
  //   init_p->thread.regs.useresp);

  for (;;) {}

  // We should not be here.
  dprintf("Dear developer, we have to talk...\n");

  return 1;
}
