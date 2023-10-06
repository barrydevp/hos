#include <kernel/fs/vfs.h>

#include <kernel/process/scheduler.h>
#include <kernel/memory/mmu.h>
#include <kernel/system/syscall.h>
#include <kernel/system/panic.h>
#include <kernel/fs/procfs.h>
#include <kernel/assert.h>
#include <kernel/spinlock.h>
#include <kernel/strerror.h>
#include <kernel/hashmap.h>
#include <kernel/string.h>
#include <kernel/libgen.h>
#include <kernel/stdio.h>

#include <kernel/printf.h>

/// The hashmap that associates a type of Filesystem `name` to its `mount` function;
static hashmap_t *vfs_filesystems;
/// The list of superblocks.
static list_head vfs_super_blocks;
/// The maximum number of filesystem types.
static const unsigned vfs_filesystems_max = 10;
/// Lock for refcount field.
static spinlock_t vfs_spinlock_refcount;
/// Spinlock for the entire virtual filesystem.
static spinlock_t vfs_spinlock;
/// VFS memory cache for superblocks.
// static kmem_cache_t *vfs_superblock_cache;

/// VFS memory cache for files.
// kmem_cache_t *vfs_file_cache;

void vfs_init() {
  // Initialize the list of superblocks.
  list_head_init(&vfs_super_blocks);
  // Initialize the caches for superblocks and files.
  // vfs_superblock_cache = KMEM_CREATE(super_block_t);
  // vfs_file_cache       = KMEM_CREATE(vfs_file_t);
  // Allocate the hashmap for the different filesystems.
  vfs_filesystems = hashmap_create(vfs_filesystems_max, hashmap_str_hash,
                                   hashmap_str_comp, hashmap_do_not_duplicate,
                                   hashmap_do_not_free);
  // Initialize the spinlock.
  spinlock_init(&vfs_spinlock);
  spinlock_init(&vfs_spinlock_refcount);
}

int vfs_register_filesystem(file_system_type *fs) {
  if (hashmap_set(vfs_filesystems, (void *)fs->name, (void *)fs) != NULL) {
    dprintf("Filesystem already registered.\n");
    return 0;
  }
  dprintf("vfs_register_filesystem(`%s`) : %p\n", fs->name, fs);
  return 1;
}

int vfs_unregister_filesystem(file_system_type *fs) {
  if (hashmap_remove(vfs_filesystems, (void *)fs->name) != NULL) {
    dprintf("Filesystem not present to unregister.\n");
    return 0;
  }
  return 1;
}

super_block_t *vfs_get_superblock(const char *absolute_path) {
  size_t last_sb_len     = 0;
  super_block_t *last_sb = NULL, *superblock = NULL;
  list_head *it;
  list_for_each(it, &vfs_super_blocks) {
    superblock = list_entry(it, super_block_t, mounts);
#if 0
        int len    = strlen(superblock->name);
        dprintf("`%s` vs `%s`\n", absolute_path, superblock->name);
        if (!strncmp(absolute_path, superblock->name, len)) {
            size_t sbl = strlen(superblock->name);
            if (sbl > last_sb_len) {
                last_sb_len = sbl;
                last_sb     = superblock;
            }
        }
#else
    int len = strlen(superblock->path);
    if (!strncmp(absolute_path, superblock->path, len)) {
      if (len > last_sb_len) {
        last_sb_len = len;
        last_sb     = superblock;
      }
    }
#endif
  }
  return last_sb;
}

vfs_file_t *vfs_open(const char *path, int flags, mode_t mode) {
  // Allocate a variable for the path.
  char absolute_path[PATH_MAX];
  // If the first character is not the '/' then get the absolute path.
  if (!realpath(path, absolute_path)) {
    dprintf("vfs_open(%s): Cannot get the absolute path!\n", path);
    errno = ENODEV;
    return NULL;
  }
  super_block_t *sb = vfs_get_superblock(absolute_path);
  if (sb == NULL) {
    dprintf("vfs_open(%s): Cannot find the superblock!\n", path);
    errno = ENOENT;
    return NULL;
  }
  vfs_file_t *sb_root = sb->root;
  if (sb_root == NULL) {
    dprintf("vfs_open(%s): Cannot find the superblock root!\n", path);
    errno = ENOENT;
    return NULL;
  }
  // Rebase the absolute path.
  //size_t name_offset = (strcmp(mp->name, "/") == 0) ? 0 : strlen(mp->name);
  // Check if the function is implemented.
  if (sb_root->fs_operations->open_f == NULL) {
    dprintf("vfs_open(%s): Function not supported in current filesystem.",
            path);
    errno = ENOSYS;
    return NULL;
  }
  // Retrieve the file.
  vfs_file_t *file = sb_root->fs_operations->open_f(absolute_path, flags, mode);
  if (file == NULL) {
    dprintf(
      "vfs_open(%s): Filesystem open returned NULL file (errno: %d, %s)!\n",
      path, errno, strerror(errno));
    return NULL;
  }
  // Increment file reference counter.
  file->count += 1;
  // Return the file.
  return file;
}

int vfs_close(vfs_file_t *file) {
  dprintf("vfs_close(ino: %d, file: \"%s\", count: %d)\n", file->ino,
          file->name, file->count - 1);
  assert(file->count > 0);
  // Close file if it's the last reference.
  if (--file->count == 0) {
    // Check if the filesystem has the close function.
    if (file->fs_operations->close_f == NULL) {
      return -ENOSYS;
    }
    file->fs_operations->close_f(file);
  }
  return 0;
}

ssize_t vfs_read(vfs_file_t *file, void *buf, size_t offset, size_t nbytes) {
  if (file->fs_operations->read_f == NULL) {
    dprintf("No READ function found for the current filesystem.\n");
    return -ENOSYS;
  }
  return file->fs_operations->read_f(file, buf, offset, nbytes);
}

ssize_t vfs_write(vfs_file_t *file, void *buf, size_t offset, size_t nbytes) {
  if (file->fs_operations->write_f == NULL) {
    dprintf("No WRITE function found for the current filesystem.\n");
    return -ENOSYS;
  }
  return file->fs_operations->write_f(file, buf, offset, nbytes);
}

off_t vfs_lseek(vfs_file_t *file, off_t offset, int whence) {
  if (file->fs_operations->lseek_f == NULL) {
    dprintf("No WRITE function found for the current filesystem.\n");
    return -ENOSYS;
  }
  return file->fs_operations->lseek_f(file, offset, whence);
}

int vfs_getdents(vfs_file_t *file, dirent_t *dirp, off_t off, size_t count) {
  if (file->fs_operations->getdents_f == NULL) {
    dprintf("No GETDENTS function found for the current filesystem.\n");
    return -ENOSYS;
  }
  return file->fs_operations->getdents_f(file, dirp, off, count);
}

int vfs_ioctl(vfs_file_t *file, int request, void *data) {
  if (file->fs_operations->ioctl_f == NULL) {
    dprintf("No IOCTL function found for the current filesystem.\n");
    return -ENOSYS;
  }
  return file->fs_operations->ioctl_f(file, request, data);
}

int vfs_unlink(const char *path) {
  // Allocate a variable for the path.
  char absolute_path[PATH_MAX];
  // If the first character is not the '/' then get the absolute path.
  if (!realpath(path, absolute_path)) {
    dprintf("vfs_unlink(%s): Cannot get the absolute path.", path);
    return -ENODEV;
  }
  super_block_t *sb = vfs_get_superblock(absolute_path);
  if (sb == NULL) {
    dprintf("vfs_unlink(%s): Cannot find the superblock!\n", path);
    return -ENODEV;
  }
  vfs_file_t *sb_root = sb->root;
  if (sb_root == NULL) {
    dprintf("vfs_unlink(%s): Cannot find the superblock root.", path);
    return -ENOENT;
  }
  // Check if the function is implemented.
  if (sb_root->fs_operations->unlink_f == NULL) {
    dprintf("vfs_unlink(%s): Function not supported in current filesystem.",
            path);
    return -ENOSYS;
  }
  return sb_root->fs_operations->unlink_f(absolute_path);
}

int vfs_mkdir(const char *path, mode_t mode) {
  // Allocate a variable for the path.
  char absolute_path[PATH_MAX];
  // If the first character is not the '/' then get the absolute path.
  if (!realpath(path, absolute_path)) {
    dprintf("vfs_mkdir(%s): Cannot get the absolute path.", path);
    return -ENODEV;
  }
  super_block_t *sb = vfs_get_superblock(absolute_path);
  if (sb == NULL) {
    dprintf("vfs_mkdir(%s): Cannot find the superblock!\n", path);
    return -ENODEV;
  }
  vfs_file_t *sb_root = sb->root;
  if (sb_root == NULL) {
    dprintf("vfs_mkdir(%s): Cannot find the superblock root.", path);
    return -ENOENT;
  }
  // Check if the function is implemented.
  if (sb_root->sys_operations->mkdir_f == NULL) {
    dprintf("vfs_mkdir(%s): Function not supported in current filesystem.",
            path);
    return -ENOSYS;
  }
  return sb_root->sys_operations->mkdir_f(absolute_path, mode);
}

int vfs_rmdir(const char *path) {
  // Allocate a variable for the path.
  char absolute_path[PATH_MAX];
  // If the first character is not the '/' then get the absolute path.
  if (!realpath(path, absolute_path)) {
    dprintf("vfs_rmdir(%s): Cannot get the absolute path.", path);
    return -ENODEV;
  }
  super_block_t *sb = vfs_get_superblock(absolute_path);
  if (sb == NULL) {
    dprintf("vfs_rmdir(%s): Cannot find the superblock!\n", path);
    return -ENODEV;
  }
  vfs_file_t *sb_root = sb->root;
  if (sb_root == NULL) {
    dprintf("vfs_rmdir(%s): Cannot find the superblock root.", path);
    return -ENOENT;
  }
  // Check if the function is implemented.
  if (sb_root->sys_operations->rmdir_f == NULL) {
    dprintf("vfs_rmdir(%s): Function not supported in current filesystem.",
            path);
    return -ENOSYS;
  }
  // Remove the file.
  return sb_root->sys_operations->rmdir_f(absolute_path);
}

vfs_file_t *vfs_create(const char *path, mode_t mode) {
  // Allocate a variable for the path.
  char absolute_path[PATH_MAX];
  // If the first character is not the '/' then get the absolute path.
  if (!realpath(path, absolute_path)) {
    dprintf("vfs_creat(%s): Cannot get the absolute path.", path);
    errno = ENODEV;
    return NULL;
  }
  super_block_t *sb = vfs_get_superblock(absolute_path);
  if (sb == NULL) {
    dprintf("vfs_creat(%s): Cannot find the superblock!\n", path);
    errno = ENODEV;
    return NULL;
  }
  vfs_file_t *sb_root = sb->root;
  if (sb_root == NULL) {
    dprintf("vfs_creat(%s): Cannot find the superblock root.", path);
    errno = ENOENT;
    return NULL;
  }
  // Check if the function is implemented.
  if (sb_root->sys_operations->creat_f == NULL) {
    dprintf("vfs_creat(%s): Function not supported in current filesystem.",
            path);
    errno = ENOSYS;
    return NULL;
  }
  // Retrieve the file.
  vfs_file_t *file = sb_root->sys_operations->creat_f(absolute_path, mode);
  if (file == NULL) {
    dprintf("vfs_open(%s): Cannot find the given file (%s)!\n", path,
            strerror(errno));
    errno = ENOENT;
    return NULL;
  }
  // Increment file reference counter.
  file->count += 1;
  // Return the file.
  return file;
}

int vfs_stat(const char *path, stat_t *buf) {
  // Allocate a variable for the path.
  char absolute_path[PATH_MAX];
  // If the first character is not the '/' then get the absolute path.
  if (!realpath(path, absolute_path)) {
    dprintf("vfs_stat(%s): Cannot get the absolute path.", path);
    return -ENODEV;
  }
  super_block_t *sb = vfs_get_superblock(absolute_path);
  if (sb == NULL) {
    dprintf("vfs_stat(%s): Cannot find the superblock!\n", path);
    return -ENODEV;
  }
  vfs_file_t *sb_root = sb->root;
  if (sb_root == NULL) {
    dprintf("vfs_stat(%s): Cannot find the superblock root.", path);
    return -ENOENT;
  }
  // Check if the function is implemented.
  if (sb_root->sys_operations->stat_f == NULL) {
    dprintf("vfs_stat(%s): Function not supported in current filesystem.",
            path);
    return -ENOSYS;
  }
  // Reset the structure.
  buf->st_dev   = 0;
  buf->st_ino   = 0;
  buf->st_mode  = 0;
  buf->st_uid   = 0;
  buf->st_gid   = 0;
  buf->st_size  = 0;
  buf->st_atime = 0;
  buf->st_mtime = 0;
  buf->st_ctime = 0;
  // Retrieve the file.
  return sb_root->sys_operations->stat_f(absolute_path, buf);
}

int vfs_fstat(vfs_file_t *file, stat_t *buf) {
  if (file->fs_operations->stat_f == NULL) {
    dprintf("No FSTAT function found for the current filesystem.\n");
    return -ENOSYS;
  }
  // Reset the structure.
  buf->st_dev   = 0;
  buf->st_ino   = 0;
  buf->st_mode  = 0;
  buf->st_uid   = 0;
  buf->st_gid   = 0;
  buf->st_size  = 0;
  buf->st_atime = 0;
  buf->st_mtime = 0;
  buf->st_ctime = 0;
  return file->fs_operations->stat_f(file, buf);
}

int vfs_mount(const char *path, vfs_file_t *new_fs_root) {
  if (!path || path[0] != '/') {
    dprintf("vfs_mount(%s): Path must be absolute for superblock.\n", path);
    return 0;
  }
  if (new_fs_root == NULL) {
    dprintf("vfs_mount(%s): You must provide a valid file!\n", path);
    return 0;
  }
  // Lock the vfs spinlock.
  spinlock_lock(&vfs_spinlock);
  dprintf("Mounting file with path `%s` as root '%s'...\n", new_fs_root->name,
          path);
  // Create the superblock.
  // super_block_t *sb = kmem_cache_alloc(vfs_superblock_cache, GFP_KERNEL);
  super_block_t *sb = kmalloc(sizeof(super_block_t));
  if (!sb) {
    dprintf("Cannot allocate memory for the superblock.\n");
    return 0;
  } else {
    // Copy the name.
    strcpy(sb->name, new_fs_root->name);
    // Copy the path.
    strcpy(sb->path, path);
    // Set the pointer.
    sb->root = new_fs_root;
    // Add to the list.
    list_head_insert_after(&sb->mounts, &vfs_super_blocks);
  }
  spinlock_unlock(&vfs_spinlock);
  dprintf("Correctly mounted '%s' on '%s'...\n", new_fs_root->name, path);
  return 1;
}

int do_mount(const char *type, const char *path, const char *args) {
  file_system_type *fst =
    (file_system_type *)hashmap_get(vfs_filesystems, type);
  if (fst == NULL) {
    dprintf("Unknown filesystem type: %s\n", type);
    return -ENODEV;
  }
  if (fst->mount == NULL) {
    dprintf("No mount callback set: %s\n", type);
    return -ENODEV;
  }
  vfs_file_t *file = fst->mount(path, args);
  if (file == NULL) {
    dprintf("Mount callback return a null pointer: %s\n", type);
    return -ENODEV;
  }
  if (!vfs_mount(path, file)) {
    dprintf("do_mount(`%s`, `%s`, `%s`) : failed to mount.\n", type, path,
            args);
    return -ENODEV;
  }
  super_block_t *sb = vfs_get_superblock(path);
  if (sb == NULL) {
    dprintf("do_mount(`%s`, `%s`, `%s`) : Cannot find the superblock.\n", type,
            path, args);
    return -ENODEV;
  }
  // Set the filesystem type.
  sb->type = fst;
  dprintf("Mounted %s[%s] to `%s`: file = %p\n", type, args, path, file);
  return 0;
}

void vfs_lock(vfs_file_t *file) {
  spinlock_lock(&vfs_spinlock_refcount);
  file->refcount = -1;
  spinlock_unlock(&vfs_spinlock_refcount);
}

int vfs_extend_task_fd_list(struct task_struct *task) {
  if (!task) {
    dprintf("Null process.\n");
    errno = ESRCH;
    return 0;
  }
  // Set the max number of file descriptors.
  int new_max_fd = (task->fd_list) ? task->max_fd * 2 + 1 : MAX_OPEN_FD;
  // Allocate the memory for the list.
  void *new_fd_list = kmalloc(new_max_fd * sizeof(vfs_file_descriptor_t));
  // Check the new list.
  if (!new_fd_list) {
    dprintf("Failed to allocate memory for `fd_list`.\n");
    errno = EMFILE;
    return 0;
  }
  // Clear the memory of the new list.
  memset(new_fd_list, 0, task->max_fd * sizeof(vfs_file_descriptor_t));
  // Deal with pre-existing list.
  if (task->fd_list) {
    // Copy the old entries.
    memcpy(new_fd_list, task->fd_list,
           task->max_fd * sizeof(vfs_file_descriptor_t));
    // Free the memory of the old list.
    kfree(task->fd_list);
  }
  // Set the new maximum number of file descriptors.
  task->max_fd = new_max_fd;
  // Set the new list.
  task->fd_list = new_fd_list;
  return 1;
}

int vfs_init_task(task_struct *task) {
  if (!task) {
    dprintf("Null process.\n");
    errno = ESRCH;
    return 0;
  }
  // Initialize the file descriptor list.
  if (!vfs_extend_task_fd_list(task)) {
    dprintf(
      "Error while trying to initialize the `fd_list` for process `%d`: %s\n",
      task->pid, strerror(errno));
    return 0;
  }
  // Create the proc entry.
  if (procr_create_entry_pid(task)) {
    dprintf("Error while trying to create proc entry for process `%d`: %s\n",
            task->pid, strerror(errno));
    return 0;
  }
  return 1;
}

int vfs_dup_task(task_struct *task, task_struct *old_task) {
  // Copy the maximum number of file descriptors.
  task->max_fd = old_task->max_fd;
  // Allocate the memory for the new list.
  task->fd_list = kmalloc(task->max_fd * sizeof(vfs_file_descriptor_t));
  // Copy the old list.
  memcpy(task->fd_list, old_task->fd_list,
         task->max_fd * sizeof(vfs_file_descriptor_t));
  // Increase the counters to the open files.
  for (int fd = 0; fd < task->max_fd; fd++) {
    // Check if the file descriptor is associated with a file.
    if (task->fd_list[fd].file_struct) {
      // Increase the counter.
      ++task->fd_list[fd].file_struct->count;
    }
  }
  // Create the proc entry.
  if (procr_create_entry_pid(task)) {
    dprintf("Error while trying to create proc entry for '%d': %s\n", task->pid,
            strerror(errno));
    return 0;
  }
  return 1;
}

int vfs_destroy_task(task_struct *task) {
  // Decrease the counters to the open files.
  for (int fd = 0; fd < task->max_fd; fd++) {
    // Check if the file descriptor is associated with a file.
    if (task->fd_list[fd].file_struct) {
      // Decrease the counter.
      --task->fd_list[fd].file_struct->count;
      // If counter is zero, close the file.
      if (task->fd_list[fd].file_struct->count == 0)
        task->fd_list[fd].file_struct->fs_operations->close_f(
          task->fd_list[fd].file_struct);
      // Clear the pointer to the file structure.
      task->fd_list[fd].file_struct = NULL;
    }
  }
  // Set the maximum file descriptors to 0.
  task->max_fd = 0;
  // Free the memory of the list.
  kfree(task->fd_list);
  // Remove the proc entry.
  if (procr_destroy_entry_pid(task)) {
    dprintf("Error while trying to remove proc entry for '%d': %s\n", task->pid,
            strerror(errno));
    return 0;
  }
  return 1;
}
