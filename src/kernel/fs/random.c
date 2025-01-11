/// Copyright (C) 2014-2018 K. Lange
/// Copyright (c) 2022-2025 Minh Hai Dao (barrydevp)

#include <kernel/fs/modules.h>
#include <kernel/fs/vfs.h>
#include <kernel/fcntl.h>
#include <kernel/memory/mmu.h>
#include <kernel/stdint.h>
#include <kernel/string.h>

#include <kernel/printf.h>

#define RANDOMFS_MAGIC_NUMBER 0xC0

vfs_file_t *random_fs = NULL;

uint32_t rand(void) {
  static uint32_t x = 123456789;
  static uint32_t y = 362436069;
  static uint32_t z = 521288629;
  static uint32_t w = 88675123;

  uint32_t t;

  t = x ^ (x << 11);
  x = y;
  y = z;
  z = w;

  return w = w ^ (w >> 19) ^ t ^ (t >> 8);
}

static ssize_t read_random(vfs_file_t *node, char *buffer, off_t offset,
                           size_t size) {
  size_t s = 0;
  while (s < size) {
    buffer[s] = rand() % 0xFF;
    s++;
  }
  return size;
}

static vfs_file_t *open_random(const char *path, int flags, mode_t mode) {
  return random_fs;
}

static int close_random(vfs_file_t *file) {
  return 0;
}

/// Filesystem file operations.
static vfs_sys_operations_t randomfs_sys_operations = {};

static vfs_file_operations_t randomfs_fs_operations = {
  .read_f  = read_random,
  .open_f  = open_random,
  .close_f = close_random,
};

static vfs_file_t *random_device_create(void) {
  // vfs_file_t *fnode = kmem_cache_alloc(vfs_file_cache, GFP_KERNEL);
  vfs_file_t *fnode = kmalloc(sizeof(vfs_file_t));
  if (!fnode) {
    dprintf(
      "random_device_create(): Failed to allocate memory for VFS file!\n");
    return NULL;
  }

  memset(fnode, 0, sizeof(vfs_file_t));

  strcpy(fnode->name, "random");
  fnode->device         = NULL;
  fnode->ino            = 0;
  fnode->uid            = 0;
  fnode->gid            = 0;
  fnode->mask           = S_IRUSR | S_IRGRP | S_IROTH;
  fnode->length         = 1024;
  fnode->flags          = fnode->flags;
  fnode->sys_operations = &randomfs_sys_operations;
  fnode->fs_operations  = &randomfs_fs_operations;

  dprintf("random_device_create(): VFS file : %p\n", fnode);

  return random_fs = fnode;
}

int random_init() {
  vfs_file_t *fnode = random_device_create();
  vfs_mount("/dev/random", fnode);
  vfs_mount("/dev/urandom", fnode);

  return 0;
}
