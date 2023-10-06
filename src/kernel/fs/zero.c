#include <kernel/fs/vfs.h>
#include <kernel/memory/mmu.h>
#include <kernel/fcntl.h>
#include <kernel/stdint.h>
#include <kernel/stddef.h>
#include <kernel/errno.h>
#include <kernel/string.h>

#include <kernel/printf.h>

vfs_file_t *null_fs = NULL;
vfs_file_t *zero_fs = NULL;

static ssize_t read_null(vfs_file_t *file, char *buf, off_t offset,
                         size_t nbyte) {
  return 0;
}

static ssize_t write_null(vfs_file_t *file, const void *buf, off_t offset,
                          size_t nbyte) {
  return nbyte;
}

static vfs_file_t *open_null(const char *path, int flags, mode_t mode) {
  return null_fs;
}

static int close_null(vfs_file_t *file) {
  return 0;
}

static ssize_t read_zero(vfs_file_t *file, char *buffer, off_t offset,
                         size_t size) {
  memset(buffer, 0x00, size);
  return size;
}

static ssize_t write_zero(vfs_file_t *file, const void *buf, off_t offset,
                          size_t nbyte) {
  return nbyte;
}

static vfs_file_t *open_zero(const char *path, int flags, mode_t mode) {
  return zero_fs;
}

static int close_zero(vfs_file_t *file) {
  return 0;
}

/// Filesystem file operations.
static vfs_sys_operations_t nullfs_sys_operations = {};

static vfs_file_operations_t nullfs_fs_operations = {
  .read_f  = read_null,
  .write_f = write_null,
  .open_f  = open_null,
  .close_f = close_null,
};

static vfs_sys_operations_t zerofs_sys_operations = {};

static vfs_file_operations_t zerofs_fs_operations = {
  .read_f  = read_zero,
  .write_f = write_zero,
  .open_f  = open_zero,
  .close_f = close_zero,
};

static vfs_file_t *null_device_create(void) {
  // vfs_file_t *fnode = kmem_cache_alloc(vfs_file_cache, GFP_KERNEL);
  vfs_file_t *fnode = kmalloc(sizeof(vfs_file_t));
  if (!fnode) {
    dprintf("null_device_create(%p): Failed to allocate memory for VFS file!\n",
            fnode);
    return NULL;
  }

  memset(fnode, 0, sizeof(vfs_file_t));

  strcpy(fnode->name, "null");
  fnode->device = NULL;
  fnode->ino    = 0;
  fnode->uid    = 0;
  fnode->gid    = 0;
  fnode->mask   = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH;
  fnode->length = 1024;
  fnode->flags  = fnode->flags;
  fnode->sys_operations = &nullfs_sys_operations;
  fnode->fs_operations  = &nullfs_fs_operations;

  dprintf("null_device_create(%p): VFS file : %p\n", fnode);

  return null_fs = fnode;
}

static vfs_file_t *zero_device_create(void) {
  // vfs_file_t *fnode = kmem_cache_alloc(vfs_file_cache, GFP_KERNEL);
  vfs_file_t *fnode = kmalloc(sizeof(vfs_file_t));
  if (!fnode) {
    dprintf("zero_device_create(%p): Failed to allocate memory for VFS file!\n",
            fnode);
    return NULL;
  }

  memset(fnode, 0, sizeof(vfs_file_t));

  strcpy(fnode->name, "zero");
  fnode->device = NULL;
  fnode->ino    = 0;
  fnode->uid    = 0;
  fnode->gid    = 0;
  fnode->mask   = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH;
  fnode->length = 1024;
  fnode->flags  = fnode->flags;
  fnode->sys_operations = &zerofs_sys_operations;
  fnode->fs_operations  = &zerofs_fs_operations;

  dprintf("zero_device_create(%p): VFS file : %p\n", fnode);

  return zero_fs = fnode;
}

int zero_module_init() {
  vfs_mount("/dev/null", null_device_create());
  vfs_mount("/dev/zero", zero_device_create());

  return 0;
}
