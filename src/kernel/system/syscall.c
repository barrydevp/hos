#include <arch/i386/fpu.h>
#include <arch/i386/irq.h>
#include <arch/i386/timer.h>
#include <kernel/memory/mmu.h>
#include <kernel/system/syscall.h>
#include <kernel/errno.h>
#include <kernel/kernel.h>
#include <kernel/process/task.h>
#include <kernel/process/scheduler.h>
// #include <kernel/sys/utsname.h>
// #include <kernel/fs/ioctl.h>
#include <kernel/printf.h>

// #include "ipc/msg.h"
// #include "ipc/sem.h"
// #include "ipc/shm.h"

/// The signature of a function call.
typedef int (*syscall_func)();

/// The list of function call.
syscall_func syscalls[SYSCALL_NUMBER];

/// Last interupt stack frame
static pt_regs *current_interrupt_stack_frame;

/// @brief A Not Implemented (NI) system-call.
/// @return Always returns -ENOSYS.
/// @details
/// Linux provides a "not implemented" system call, sys_ni_syscall(), which does
/// nothing except return ENOSYS, the error corresponding to an invalid
/// system call. This function is used to "plug the hole" in the rare event that
/// a syscall is removed or otherwise made unavailable.
static inline int sys_ni_syscall() {
  return -ENOSYS;
}

int call0() {
  return 65;
}

int call1(int a) {
  return a;
}

int print(char *s) {
  dprintf("syscall_print: %s\n", s);
  printf("syscall_print: %s\n", s);

  return 0;
}

void syscall_init() {
  // Initialize the list of system calls.
  for (uint32_t it = 0; it < SYSCALL_NUMBER; ++it) {
    syscalls[it] = sys_ni_syscall;
  }

  syscalls[20] = (syscall_func)call0;
  syscalls[21] = (syscall_func)call1;
  syscalls[22] = (syscall_func)print;

  // syscalls[__NR_exit]           = (syscall_func)sys_exit;
  // syscalls[__NR_read]           = (syscall_func)sys_read;
  // syscalls[__NR_write]          = (syscall_func)sys_write;
  // syscalls[__NR_open]           = (syscall_func)sys_open;
  // syscalls[__NR_close]          = (syscall_func)sys_close;
  // syscalls[__NR_stat]           = (syscall_func)sys_stat;
  // syscalls[__NR_fstat]          = (syscall_func)sys_fstat;
  // syscalls[__NR_mkdir]          = (syscall_func)sys_mkdir;
  // syscalls[__NR_rmdir]          = (syscall_func)sys_rmdir;
  // syscalls[__NR_creat]          = (syscall_func)sys_creat;
  // syscalls[__NR_unlink]         = (syscall_func)sys_unlink;
  // syscalls[__NR_getdents]       = (syscall_func)sys_getdents;
  // syscalls[__NR_lseek]          = (syscall_func)sys_lseek;
  // syscalls[__NR_getpid]         = (syscall_func)sys_getpid;
  // syscalls[__NR_getsid]         = (syscall_func)sys_getsid;
  // syscalls[__NR_setsid]         = (syscall_func)sys_setsid;
  // syscalls[__NR_getpgid]        = (syscall_func)sys_getpgid;
  // syscalls[__NR_setpgid]        = (syscall_func)sys_setpgid;
  // syscalls[__NR_getuid]         = (syscall_func)sys_getuid;
  // syscalls[__NR_setuid]         = (syscall_func)sys_setuid;
  // syscalls[__NR_getgid]         = (syscall_func)sys_getgid;
  // syscalls[__NR_setgid]         = (syscall_func)sys_setgid;
  // syscalls[__NR_getppid]        = (syscall_func)sys_getppid;
  // syscalls[__NR_sigaction]      = (syscall_func)sys_sigaction;
  // syscalls[__NR_fork]           = (syscall_func)sys_fork;
  // syscalls[__NR_execve]         = (syscall_func)sys_execve;
  // syscalls[__NR_nice]           = (syscall_func)sys_nice;
  // syscalls[__NR_kill]           = (syscall_func)sys_kill;
  // syscalls[__NR_reboot]         = (syscall_func)sys_reboot;
  // syscalls[__NR_uname]          = (syscall_func)sys_uname;
  // syscalls[__NR_sigreturn]      = (syscall_func)sys_sigreturn;
  // syscalls[__NR_waitpid]        = (syscall_func)sys_waitpid;
  // syscalls[__NR_chdir]          = (syscall_func)sys_chdir;
  // syscalls[__NR_fchdir]         = (syscall_func)sys_fchdir;
  // syscalls[__NR_time]           = (syscall_func)sys_time;
  // syscalls[__NR_sigprocmask]    = (syscall_func)sys_sigprocmask;
  // syscalls[__NR_brk]            = (syscall_func)sys_brk;
  // syscalls[__NR_signal]         = (syscall_func)sys_signal;
  // syscalls[__NR_ioctl]          = (syscall_func)sys_ioctl;
  // syscalls[__NR_sched_setparam] = (syscall_func)sys_sched_setparam;
  // syscalls[__NR_sched_getparam] = (syscall_func)sys_sched_getparam;
  // syscalls[__NR_nanosleep]      = (syscall_func)sys_nanosleep;
  // syscalls[__NR_getcwd]         = (syscall_func)sys_getcwd;
  // syscalls[__NR_waitperiod]     = (syscall_func)sys_waitperiod;
  // syscalls[__NR_msgctl]         = (syscall_func)sys_msgctl;
  // syscalls[__NR_msgget]         = (syscall_func)sys_msgget;
  // syscalls[__NR_msgrcv]         = (syscall_func)sys_msgrcv;
  // syscalls[__NR_msgsnd]         = (syscall_func)sys_msgsnd;
  // syscalls[__NR_semctl]         = (syscall_func)sys_semctl;
  // syscalls[__NR_semget]         = (syscall_func)sys_semget;
  // syscalls[__NR_semop]          = (syscall_func)sys_semop;
  // syscalls[__NR_shmat]          = (syscall_func)sys_shmat;
  // syscalls[__NR_shmctl]         = (syscall_func)sys_shmctl;
  // syscalls[__NR_shmdt]          = (syscall_func)sys_shmdt;
  // syscalls[__NR_shmget]         = (syscall_func)sys_shmget;
  // syscalls[__NR_alarm]          = (syscall_func)sys_alarm;
  // syscalls[__NR_setitimer]      = (syscall_func)sys_setitimer;
  // syscalls[__NR_getitimer]      = (syscall_func)sys_getitimer;

  // isr_install_handler(SYSTEM_CALL, &syscall_handler, "syscall_handler");
}

pt_regs *get_current_interrupt_stack_frame() {
  return current_interrupt_stack_frame;
}

void syscall_handler(pt_regs *f) {
  // Saves current interrupt stack frame
  current_interrupt_stack_frame = f;

  // dbg_print_regs(f);
  // // Save current process fpu state.
  // switch_fpu();

  // The index of the requested system call.
  uint32_t sc_index = f->eax;

  // The result of the system call.
  int ret;
  if (sc_index >= SYSCALL_NUMBER) {
    ret = ENOSYS;
  } else {
    uintptr_t ptr = (uintptr_t)syscalls[sc_index];

    syscall_func func = (syscall_func)ptr;

    uint32_t arg0 = f->ebx;
    uint32_t arg1 = f->ecx;
    uint32_t arg2 = f->edx;
    uint32_t arg3 = f->esi;
    uint32_t arg4 = f->edi;
    if ((sc_index == __NR_fork) || (sc_index == __NR_clone) ||
        (sc_index == __NR_execve) || (sc_index == __NR_sigreturn)) {
      arg0 = (uintptr_t)f;
    }
    ret = func(arg0, arg1, arg2, arg3, arg4);
  }
  f->eax = ret;

  // // Schedule next process.
  // scheduler_run(f);
  // // Restore fpu state.
  // unswitch_fpu();
}
