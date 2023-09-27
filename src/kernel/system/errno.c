#include <kernel/errno.h>
#include <kernel/process/scheduler.h>

/// @brief Returns the error number for the current process.
/// @return Pointer to the error number.
int *__geterrno() {
  static int _errno            = 0;
  task_struct *current_process = scheduler_get_current_process();
  if (current_process == NULL) {
    return &_errno;
  }
  return &current_process->error_no;
}
