#include <kernel/process/scheduler.h>
#include <kernel/drivers/keyboard.h>
#include <kernel/drivers/keyboard/keymap.h>
#include <kernel/drivers/video.h>
#include <kernel/fs/vfs.h>
#include <kernel/fs/procfs.h>
#include <kernel/termios.h>
#include <kernel/ioctls.h>
#include <kernel/bitops.h>
#include <kernel/errno.h>
#include <kernel/fcntl.h>
#include <kernel/ctype.h>

#include <kernel/printf.h>

/// @brief Prints the ring-buffer.
/// @param rb the ring-buffer to print.
void print_rb(fs_rb_scancode_t *rb) {
  if (!fs_rb_scancode_empty(rb)) {
    for (unsigned i = rb->read; (i < rb->write) && (i < rb->size); ++i) {
      dprintf("%c", rb->buffer[i]);
    }
    dprintf("\n");
  }
}

static ssize_t procv_read(vfs_file_t *file, char *buf, off_t offset,
                          size_t nbyte) {
  // Stop if the buffer is invalid.
  if (buf == NULL)
    return -1;

  // Get the currently running process.
  task_struct *process = scheduler_get_current_process();
  // Get a pointer to its ketboard ring-buffer.
  fs_rb_scancode_t *rb = &process->keyboard_rb;
  // Pre-check the flags.
  bool_t flg_icanon = bitmask_check(process->termios.c_lflag, ICANON) == ICANON;
  bool_t flg_echoe  = bitmask_check(process->termios.c_lflag, ECHOE) == ECHOE;
  bool_t flg_echo   = bitmask_check(process->termios.c_lflag, ECHO) == ECHO;
  bool_t flg_isig   = bitmask_check(process->termios.c_lflag, ISIG) == ISIG;

  // If we are in canonical mode, and the last inserted element is a newline,
  // we pop the buffer until it's empty.
  if (!fs_rb_scancode_empty(rb) &&
      (!flg_icanon || (flg_icanon && (fs_rb_scancode_front(rb) == '\n')))) {
    *((char *)buf) = fs_rb_scancode_pop_back(rb) & 0x00FF;
    return 1;
  }

  // Once we have dealt with the canonical mode, get the character.
  int c = keyboard_pop_back();

  // Check that it's a valid caracter.
  if (c < 0)
    return 0;

  // Keep only the character not the scancode.
  c &= 0x00FF;

  // We just received backspace.
  if (c == '\b') {
    // If !ECHOE and ECHO, We need to show the the `^?` string.
    if (!flg_echoe && flg_echo) {
      video_puts("^?");
    }
    // If we are in canonical mode, we pop the previous character.
    if (flg_icanon) {
      // Pop the previous character in buffer.
      fs_rb_scancode_pop_front(rb);
      // Delete the previous character on video.
      if (flg_echoe)
        video_putc(c);
    } else {
      // Add the character to the buffer.
      fs_rb_scancode_push_front(rb, c);
      // Return the character.
      *((char *)buf) = fs_rb_scancode_pop_back(rb) & 0x00FF;
      return 1;
    }
    return 0;
  } else if (c == 0x7f) {
    if (flg_echo) {
      video_puts("^[[3~");
    }
    // Add the character to the buffer.
    fs_rb_scancode_push_front(rb, '\033');
    fs_rb_scancode_push_front(rb, '[');
    fs_rb_scancode_push_front(rb, '3');
    fs_rb_scancode_push_front(rb, '~');
    return 0;
  } else {
    // Add the character to the buffer.
    fs_rb_scancode_push_front(rb, c);
  }

  // If echo is activated, output the character to video.
  if (flg_echo) {
    if (iscntrl(c) && (isalpha('A' + (c - 1)) && (c != '\n') && (c != '\b'))) {
      video_putc('^');
      video_putc('A' + (c - 1));
    } else {
      video_putc(c);
    }
  }

  if (flg_isig) {
    if (iscntrl(c)) {
      if (c == 0x03) {
        sys_kill(process->pid, SIGTERM);
      } else if (c == 0x1A) {
        sys_kill(process->pid, SIGSTOP);
      }
    }
  }

  // If we are NOT in canonical mode, we can send the character back to user
  // right away.
  if (!flg_icanon) {
    *((char *)buf) = fs_rb_scancode_pop_back(rb) & 0x00FF;
    return 1;
  }

#if 0

    // The last inserted character.
    int back_c = keyboard_back();

    if (back_c < 0)
        return 0;

    // The first inserted character.
    int front_c = keyboard_front();

    dprintf("'%c' (%3d %04x) [F: '%c' (%04x)]\n", back_c, back_c, back_c, front_c, front_c);

    // Echo the character to video.
    if (flg_echo) {
        video_putc(back_c & 0x00FF);
    }

    // If we have the canonical input active, we should not return characters,
    // until we receive a newline.
    if ((flg_icanon && (front_c == '\n')) ||
        !flg_icanon) {
        *((char *)buf) = keyboard_pop_back() & 0x00FF;
        return 1;
    }
#endif

#if 0
    // Read the character from the keyboard.
    int c = keyboard_getc(false) & 0x00FF;
    if (c < 0)
        return 0;
    if (c == KEY_PAGE_UP) {
        video_shift_one_page_down();
        return 0;
    } else if (c == KEY_PAGE_DOWN) {
        video_shift_one_page_up();
        return 0;
    } else {
        // Echo the character to video.
        if (flg_echo) {
            video_putc(c & 0x00FF);
        }
    }
#endif
  return 0;
}

static ssize_t procv_write(vfs_file_t *file, const void *buf, off_t offset,
                           size_t nbyte) {
  for (size_t i = 0; i < nbyte; ++i) { video_putc(((char *)buf)[i]); }
  return nbyte;
}

static int procv_fstat(vfs_file_t *file, stat_t *stat) {
  return -ENOSYS;
}

static int procv_ioctl(vfs_file_t *file, int request, void *data) {
  task_struct *process = scheduler_get_current_process();
  switch (request) {
    case TCGETS:
      *((termios_t *)data) = process->termios;
      break;
    case TCSETS:
      process->termios = *((termios_t *)data);
      break;
    default:
      break;
  }
  return 0;
}

// clang-format off
/// Filesystem general operations.
static vfs_sys_operations_t procv_sys_operations = {
  .mkdir_f = NULL,
  .rmdir_f = NULL,
  .stat_f  = NULL
};

/// Filesystem file operations.
static vfs_file_operations_t procv_fs_operations = {
  .open_f     = NULL,
  .unlink_f   = NULL,
  .close_f    = NULL,
  .read_f     = procv_read,
  .write_f    = procv_write,
  .lseek_f    = NULL,
  .stat_f     = procv_fstat,
  .ioctl_f    = procv_ioctl,
  .getdents_f = NULL
};
// clang-format on

int procv_module_init() {
  proc_dir_entry_t *video = proc_create_entry("video", NULL);
  if (video == NULL) {
    dprintf("Cannot create `/proc/video`.\n");
    return 1;
  }
  dprintf("Created `/proc/video` (%p)\n", video);
  // Set the specific operations.
  video->sys_operations = &procv_sys_operations;
  video->fs_operations  = &procv_fs_operations;
  return 0;
}
