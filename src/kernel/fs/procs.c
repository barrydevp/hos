#include <arch/i386/timer.h>

#include <kernel/process/task.h>
#include <kernel/memory/pmm.h>
#include <kernel/fs/procfs.h>
#include <kernel/version.h>
#include <kernel/string.h>
#include <kernel/stdio.h>
#include <kernel/errno.h>

#include <kernel/printf.h>

static ssize_t procs_do_uptime(char *buffer, size_t bufsize);

static ssize_t procs_do_version(char *buffer, size_t bufsize);

static ssize_t procs_do_mounts(char *buffer, size_t bufsize);

static ssize_t procs_do_cpuinfo(char *buffer, size_t bufsize);

static ssize_t procs_do_meminfo(char *buffer, size_t bufsize);

static ssize_t procs_do_stat(char *buffer, size_t bufsize);

static ssize_t procs_read(vfs_file_t *file, char *buf, off_t offset,
                          size_t nbyte) {
  if (file == NULL)
    return -EFAULT;
  proc_dir_entry_t *entry = (proc_dir_entry_t *)file->device;
  if (entry == NULL)
    return -EFAULT;
  // Prepare a buffer.
  char buffer[BUFSIZ];
  memset(buffer, 0, BUFSIZ);
  // Call the specific function.
  int ret = 0;
  if (strcmp(entry->name, "uptime") == 0)
    ret = procs_do_uptime(buffer, BUFSIZ);
  else if (strcmp(entry->name, "version") == 0)
    ret = procs_do_version(buffer, BUFSIZ);
  else if (strcmp(entry->name, "mounts") == 0)
    ret = procs_do_mounts(buffer, BUFSIZ);
  else if (strcmp(entry->name, "cpuinfo") == 0)
    ret = procs_do_cpuinfo(buffer, BUFSIZ);
  else if (strcmp(entry->name, "meminfo") == 0)
    ret = procs_do_meminfo(buffer, BUFSIZ);
  else if (strcmp(entry->name, "stat") == 0)
    ret = procs_do_stat(buffer, BUFSIZ);
  // Perform read.
  ssize_t it = 0;
  if (ret == 0) {
    size_t name_len = strlen(buffer);
    size_t read_pos = offset;
    if (read_pos < name_len) {
      while ((it < nbyte) && (read_pos < name_len)) {
        *buf++ = buffer[read_pos];
        ++read_pos;
        ++it;
      }
    }
  }
  return it;
}

/// Filesystem general operations.
static vfs_sys_operations_t procs_sys_operations = { .mkdir_f = NULL,
                                                     .rmdir_f = NULL,
                                                     .stat_f  = NULL };

/// Filesystem file operations.
static vfs_file_operations_t procs_fs_operations = { .open_f     = NULL,
                                                     .unlink_f   = NULL,
                                                     .close_f    = NULL,
                                                     .read_f     = procs_read,
                                                     .write_f    = NULL,
                                                     .lseek_f    = NULL,
                                                     .stat_f     = NULL,
                                                     .ioctl_f    = NULL,
                                                     .getdents_f = NULL };

int procs_init() {
  proc_dir_entry_t *system_entry;
  // == /proc/uptime ========================================================
  if ((system_entry = proc_create_entry("uptime", NULL)) == NULL) {
    dprintf("Cannot create `/proc/uptime`.\n");
    return 1;
  }
  dprintf("Created `/proc/uptime` (%p)\n", system_entry);
  // Set the specific operations.
  system_entry->sys_operations = &procs_sys_operations;
  system_entry->fs_operations  = &procs_fs_operations;

  // == /proc/version ========================================================
  if ((system_entry = proc_create_entry("version", NULL)) == NULL) {
    dprintf("Cannot create `/proc/version`.\n");
    return 1;
  }
  dprintf("Created `/proc/version` (%p)\n", system_entry);
  // Set the specific operations.
  system_entry->sys_operations = &procs_sys_operations;
  system_entry->fs_operations  = &procs_fs_operations;

  // == /proc/mounts ========================================================
  if ((system_entry = proc_create_entry("mounts", NULL)) == NULL) {
    dprintf("Cannot create `/proc/mounts`.\n");
    return 1;
  }
  dprintf("Created `/proc/mounts` (%p)\n", system_entry);
  // Set the specific operations.
  system_entry->sys_operations = &procs_sys_operations;
  system_entry->fs_operations  = &procs_fs_operations;

  // == /proc/cpuinfo ========================================================
  if ((system_entry = proc_create_entry("cpuinfo", NULL)) == NULL) {
    dprintf("Cannot create `/proc/cpuinfo`.\n");
    return 1;
  }
  dprintf("Created `/proc/cpuinfo` (%p)\n", system_entry);
  // Set the specific operations.
  system_entry->sys_operations = &procs_sys_operations;
  system_entry->fs_operations  = &procs_fs_operations;

  // == /proc/meminfo ========================================================
  if ((system_entry = proc_create_entry("meminfo", NULL)) == NULL) {
    dprintf("Cannot create `/proc/meminfo`.\n");
    return 1;
  }
  dprintf("Created `/proc/meminfo` (%p)\n", system_entry);
  // Set the specific operations.
  system_entry->sys_operations = &procs_sys_operations;
  system_entry->fs_operations  = &procs_fs_operations;

  // == /proc/stat ========================================================
  if ((system_entry = proc_create_entry("stat", NULL)) == NULL) {
    dprintf("Cannot create `/proc/stat`.\n");
    return 1;
  }
  dprintf("Created `/proc/stat` (%p)\n", system_entry);
  // Set the specific operations.
  system_entry->sys_operations = &procs_sys_operations;
  system_entry->fs_operations  = &procs_fs_operations;
  return 0;
}

static ssize_t procs_do_uptime(char *buffer, size_t bufsize) {
  sprintf(buffer, "%d", timer_get_seconds());
  return 0;
}

static ssize_t procs_do_version(char *buffer, size_t bufsize) {
  sprintf(buffer, "%s version %s (site: %s) (email: %s)", OS_NAME, OS_VERSION,
          OS_SITEURL, OS_REF_EMAIL);
  return 0;
}

static ssize_t procs_do_mounts(char *buffer, size_t bufsize) {
  return 0;
}

static ssize_t procs_do_cpuinfo(char *buffer, size_t bufsize) {
  return 0;
}

static ssize_t procs_do_meminfo(char *buffer, size_t bufsize) {
  uint32_t total_space = get_total_frames() << FRAME_SHIFT,
           used_space = (get_total_frames() - get_used_frames()) << FRAME_SHIFT,
           cached_space = 0, free_space = total_space - used_space;
  total_space /= KB;
  free_space /= KB;
  cached_space /= KB;
  used_space /= KB;
  sprintf(buffer,
          "MemTotal : %12u Kb\n"
          "MemFree  : %12u Kb\n"
          "MemUsed  : %12u Kb\n"
          "Cached   : %12u Kb\n",
          total_space, free_space, used_space, cached_space);
  return 0;
}

static ssize_t procs_do_stat(char *buffer, size_t bufsize) {
  return 0;
}
