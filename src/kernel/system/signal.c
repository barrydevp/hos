#include <kernel/system/signal.h>
#include <kernel/process/wait.h>
#include <kernel/process/scheduler.h>
#include <kernel/process/task.h>
#include <kernel/memory/mmu.h>
#include <kernel/kernel.h>
#include <kernel/errno.h>
#include <kernel/assert.h>
#include <kernel/string.h>
#include <kernel/irqflags.h>
#include <kernel/bitops.h>

#include <kernel/printf.h>

/// SLAB caches for signal bits.
// static kmem_cache_t *sigqueue_cachep;

/// Contains all stopped process waiting for a continue signal
static struct wait_queue_head_t stopped_queue;

// clang-format off
static const char *sys_siglist[] = {
    "HUP",
    "INT",
    "QUIT",
    "ILL",
    "TRAP",
    "ABRT",
    "EMT",
    "FPE",
    "KILL",
    "BUS",
    "SEGV",
    "SYS",
    "PIPE",
    "ALRM",
    "TERM",
    "USR1",
    "USR2",
    "CHLD",
    "PWR",
    "WINCH",
    "URG",
    "POLL",
    "STOP",
    "TSTP",
    "CONT",
    "TTIN",
    "TTOU",
    "VTALRM",
    "PROF",
    "XCPU",
    "XFSZ",
    NULL,
};
// clang-format on

static inline void __copy_siginfo(siginfo_t *to, const siginfo_t *from) {
  memcpy(to, from, sizeof(*to));
}

static inline void __clear_siginfo(siginfo_t *info) {
  memset(info, 0, sizeof(*info));
}

static inline void __lock_task_sighand(struct task_struct *t) {
  assert(t && "Null task struct.");
  spinlock_lock(&t->sighand.siglock);
}

static inline void __unlock_task_sighand(struct task_struct *t) {
  assert(t && "Null task struct.");
  spinlock_unlock(&t->sighand.siglock);
}

static sighandler_t __get_handler(struct task_struct *t, int sig) {
  assert(t && "Null task struct.");
  return t->sighand.action[sig - 1].sa_handler;
}

static int __sig_is_ignored(struct task_struct *t, int sig) {
  // Blocked signals are never ignored, since the
  // signal handler may change by the time it is
  // unblocked.
  if (sigismember(&t->blocked, sig) || sigismember(&t->real_blocked, sig))
    return 0;
  // Get the signal handler.
  sighandler_t handler = __get_handler(t, sig);
  // Check the type of the handler.
  return (handler == SIG_IGN) && (sig != SIGCHLD);
  // TODO: do_signal() specifically checks if the handler is IGN and the signal
  //        is SIGCHLD, in that case it forces a wait for the parent, that's why
  //        here I'm also accepting as not-ignored a SIG_IGN which is a SIGCHLD.
}

/// @brief Allocate a new signal queue record.
/// @param t     The task to which the signal belongs.
/// @param sig   The signal to set.
/// @param flags Flags identifying from where we are going to take the memory.
static sigqueue_t *__sigqueue_alloc(struct task_struct *t, int sig) {
  sigqueue_t *q = NULL;
  // if ((q = kmem_cache_alloc(sigqueue_cachep, flags)) == NULL)
  if ((q = kmalloc(sizeof(sigqueue_t)))) {
    return NULL;
  }
  // Initiliaze the values.
  q->flags = 0;
  list_head_init(&q->list);
  return q;
}

static void __sigqueue_free(sigqueue_t *q) {
  if (q) {
    // kmem_cache_free(q);
    kfree(q);
  }
}

/// @brief
/// @param sig  Signal to be sent.
/// @param info The signal info
/// @param t    The process to which we send the signal.
/// @return
static int __send_signal(int sig, siginfo_t *info, struct task_struct *t) {
  // Lock the signal handling for the given task.
  __lock_task_sighand(t);
  dprintf(
    "Trying to add signal (%2d)`%s` to task (%2d)`%s`, currently pending `%d, %d`.\n",
    sig, strsignal(sig), t->pid, t->name, t->pending.signal.sig[0],
    t->pending.signal.sig[1]);
  // Check if the signal is ignored.
  if (__sig_is_ignored(t, sig)) {
    dprintf("Trying to send signal (%2d)`%s` to task (%2d)`%s`: ignored.\n",
            sig, strsignal(sig), t->pid, t->name);
    __unlock_task_sighand(t);
    return 0;
  }
  // Check if the process is in an invalid status.
  if ((t->state == EXIT_ZOMBIE) || (t->state == EXIT_DEAD)) {
    dprintf(
      "Trying to send signal (%2d)`%s` to task (%2d)`%s`: zombie or dead.\n",
      sig, strsignal(sig), t->pid, t->name);
    __unlock_task_sighand(t);
    return -EINVAL;
  }
  // sigqueue_t *q = __sigqueue_alloc(t, sig, GFP_KERNEL);
  sigqueue_t *q = kmalloc(sizeof(sigqueue_t));
  if (q == NULL) {
    __unlock_task_sighand(t);
    return -EAGAIN;
  }
  list_head_insert_before(&q->list, &t->pending.list);
  if (info != SEND_SIG_NOINFO)
    memcpy(&q->info, info, sizeof(siginfo_t));
  // Set that there is a signal pending.
  sigaddset(&t->pending.signal, sig);
  dprintf(
    "Added pending signal (%2d)`%s` to task (%2d)`%s`, pending `%d, %d`.\n",
    sig, strsignal(sig), t->pid, t->name, t->pending.signal.sig[0],
    t->pending.signal.sig[1]);
  __unlock_task_sighand(t);
  return 0;
}

static inline int __next_signal(sigpending_t *pending, sigset_t *mask) {
  dprintf("__next_signal(%p, %p)\n", pending, mask);
  assert(pending && "Null `pending` structure.");
  assert(mask && "Null `mask` structure.");
  unsigned long x;
  if ((x = bitmask_clear(pending->signal.sig[0], mask->sig[0])) != 0)
    return 1 + find_first_non_zero(x);
  if ((x = bitmask_clear(pending->signal.sig[1], mask->sig[1])) != 0)
    return 33 + find_first_non_zero(x);
  return 0;
}

static inline void __collect_signal(int sig, sigpending_t *list,
                                    siginfo_t *info) {
  dprintf("__collect_signal(%d, %p, %p)\n", sig, list, info);
  assert(list && "Null `list` structure.");
  assert(info && "Null `info` structure.");

  sigqueue_t *queue_entry = NULL;
  bool_t still_pending    = false;
  // Collect the siginfo appropriate to this signal. Check if
  // there is another siginfo for the same signal.
  list_for_each_decl(it, &list->list) {
    sigqueue_t *q = list_entry(it, sigqueue_t, list);
    dprintf("__collect_signal(%d, %p, %p) : Signal in queue : %p(%d : %s).\n",
            sig, list, info, q, q->info.si_signo, strsignal(q->info.si_signo));
    if (q->info.si_signo == sig) {
      // If the entry is already set, this means that there are several handlers
      // pending for this particular signal.
      if (queue_entry) {
        dprintf(
          "__collect_signal(%d, %p, %p) : Still pending, do not remove from set.\n",
          sig, list, info);
        still_pending = true;
        break;
      }
      // Store the entry we encounter.
      queue_entry = q;
    }
  }
  // If there are no other signals pending of the same type,
  // remove the signal from the set.
  if (!still_pending) {
    sigdelset(&list->signal, sig);
    dprintf("__collect_signal(%d, %p, %p) : Remove signal from set: %d.\n", sig,
            list, info, list->signal.sig[0]);
  }
  // If we have found an entry.
  if (queue_entry) {
    dprintf(
      "__collect_signal(%d, %p, %p) : Remove and delete sigqueue entry : %p.\n",
      sig, list, info, queue_entry);
    // Remove the entry from the queue.
    list_head_remove(&queue_entry->list);
    // Copy the details about the entry inside the info structure.
    __copy_siginfo(info, &queue_entry->info);
    // Free the memory for the queue entry.
    __sigqueue_free(queue_entry);
  } else {
    dprintf(
      "__collect_signal(%d, %p, %p) : Cannot find the signal in the queue.\n",
      sig, list, info);
    // Ok, it wasn't in the queue, zero out the info.
    __clear_siginfo(info);
    // Get the current process.
    struct task_struct *current = scheduler_get_current_process();
    assert(current && "There is no running process.");
    // Initialize the info.
    info->si_signo           = sig;
    info->si_code            = SI_USER;
    info->si_value.sival_int = 0;
    info->si_errno           = 0;
    info->si_pid             = current->pid;
    info->si_uid             = current->uid;
    info->si_addr            = NULL;
    info->si_status          = 0;
    info->si_band            = 0;
  }
}

static inline int __dequeue_signal(sigpending_t *pending, sigset_t *mask,
                                   siginfo_t *info) {
  dprintf("__dequeue_signal(%p, %p, %p)\n", pending, mask, info);
  // The dequeue_signal( ) always considers the lowest-numbered pending signal.
  // It updates the data structures to indicate that the signal is no longer
  // pending and returns its number.
  int sig = __next_signal(pending, mask);
  if ((sig > 0) && (sig < NSIG)) {
    __collect_signal(sig, pending, info);
  }
  return sig;
}

static inline int __handle_signal(int signr, siginfo_t *info, sigaction_t *ka,
                                  struct pt_regs *regs) {
  dprintf("__handle_signal(%d, %p, %p, %p)\n", signr, info, ka, regs);
  // The do_signal() function is usually only invoked when the CPU is going
  // to return in User Mode.
  struct task_struct *current = scheduler_get_current_process();
  assert(current && "There is no running process.");
  // Skip the `init` process, always.
  if (current->pid == 1) {
    errno = ESRCH;
    return 0;
  }
  // Save the previous signal mask.
  memcpy(&current->saved_sigmask, &current->blocked, sizeof(sigset_t));

  // Add the signal to the list of blocked signals.
  sigaddset(&current->blocked, signr);

  // Store the registers before setting the ones required by the signal handling.
  current->thread.signal_regs = *regs;

  // Restore the registers for the process that has set the signal.
  *regs = current->thread.regs;

  // Set the instruction pointer.
  regs->eip = (uintptr_t)ka->sa_handler;

  // If the user is also asking for the signal info, push it into the stack.
  if (bitmask_check(ka->sa_flags, SA_SIGINFO)) {
    // Move the stack so that we have space for storing the siginfo.
    regs->useresp -= sizeof(siginfo_t);
    // Save the pointer where the siginfo is stored.
    siginfo_t *siginfo_addr = (siginfo_t *)regs->useresp;
    // We push on the stack the entire siginfo.
    __copy_siginfo(siginfo_addr, info);
    // We push on the stack the pointer to the siginfo we copied on the stack.
    PUSH_VALUE_ON_STACK(regs->useresp, siginfo_addr);
  }

  // Push on the stack the signal number, first and only argument of the handler.
  PUSH_VALUE_ON_STACK(regs->useresp, signr);

  // Push on the stack the function required to handle the signal return.
  PUSH_VALUE_ON_STACK(regs->useresp, current->sigreturn_addr);

  return 1;
}

long sys_sigreturn(struct pt_regs *f) {
  dprintf("sys_sigreturn(%p)\n", f);
  struct task_struct *current = scheduler_get_current_process();
  assert(current && "There is no running process.");
  // Restore the registers before the signal handling.
  *f = current->thread.signal_regs;
  // Restore the previous signal mask.
  memcpy(&current->blocked, &current->saved_sigmask, sizeof(sigset_t));
  // Switch to process page directory
  vmm_switch_directory(current->mm->pgd);
  dprintf("sys_sigreturn(%p) : done!\n", f);
  return 0;
}

// Send signal to parent
static int __notify_parent(struct task_struct *current, int signr) {
  siginfo_t info;
  info.si_signo           = signr;
  info.si_code            = SI_KERNEL;
  info.si_value.sival_int = 0;
  info.si_errno           = 0;
  info.si_pid             = current->pid;
  info.si_uid             = current->uid;
  info.si_addr            = NULL;
  info.si_status          = 0;
  info.si_band            = 0;
  return __send_signal(signr, &info, current->parent);
}

// Removes from the pending signal queue q the pending signals corresponding to
// the bit mask mask
static void __rm_from_queue(sigset_t *mask, sigpending_t *q) {
  list_head *it, *tmp;
  list_for_each_safe(it, tmp, &q->list) {
    struct sigqueue_t *entry = list_entry(it, struct sigqueue_t, list);
    int sig                  = entry->info.si_signo;

    if (sigismember(mask, sig)) {
      list_head_remove(it);
      kfree(entry);
    }
  }
}

// We do not consider group stopping because for now we don't have thread groups
static void __do_signal_stop(struct task_struct *current, struct pt_regs *f,
                             int signr) {
  // The do_signal( ) function also sends a SIGCHLD signal to
  // the parent process of current, unless the parent has set
  // the SA_NOCLDSTOP flag of SIGCHLD.
  if (!(SA_NOCLDSTOP & current->parent->sighand.action[SIGCHLD - 1].sa_flags))
    if (__notify_parent(current, SIGCHLD) != 0)
      dprintf("Failed to notify parent with signal: %d", signr);

  // The state is now TASK_UNINTERRUPTABLE
  sleep_on(&stopped_queue);

  current->state     = TASK_STOPPED;
  current->exit_code = signr;

  scheduler_run(f);
}

int do_signal(struct pt_regs *f) {
  // The do_signal() function is usually only invoked when the CPU is going
  // to return in User Mode.
  struct task_struct *current = scheduler_get_current_process();
  if (current == NULL)
    return 0;

  // First, checks whether the function itself was triggered by an interrupt;
  // if so, it simply returns. Otherwise, if the function was triggered by an
  // exception that was raised while the process was running in User Mode,
  // the function continues executing.
  if ((f->cs & 3) != 3)
    return 0;

  // Create a siginfo.
  siginfo_t info;
  // The return code of __dequeue_signal( ) is stored in signr.
  int signr, exit_code;

  // Lock the signal handling for the given task.
  __lock_task_sighand(current);

  // The heart of the do_signal( ) function consists of a loop that
  // repeatedly invokes the __dequeue_signal( ) function until no
  // non-blocked pending signals are left.
  while (!list_head_empty(&current->pending.list)) {
    // Get the signal to deliver.
    signr = exit_code =
      __dequeue_signal(&current->pending, &current->blocked, &info);

    // Check the signal that we want to send.
    if ((signr < 0) || (signr >= NSIG)) {
      dprintf("Wrong signal number!\n");
      break;
    }

    // If its value is 0, it means that all pending signals have been
    // handled and do_signal( ) can finish.
    if (signr == 0) {
      dprintf("There are no more signals to handle.\n");
      __unlock_task_sighand(current);
      return 0;
    }

    // Get the associated signal action.
    sigaction_t *ka = &current->sighand.action[signr - 1];

    // The only exception comes when the receiving process is init, in
    // which case the signal is discarded.
    if (current->pid == 1)
      continue;

    // When a delivered signal is explicitly ignored, the do_signal( )
    // function normally just continues with a new execution of the loop
    // and therefore considers another pending signal.
    if (ka->sa_handler == SIG_IGN) {
      if (signr == SIGCHLD)
        while (sys_waitpid(-1, NULL, WNOHANG) > 0) {}
      continue;
    }

    // When a delivered signal is the default one, do_signal( ) must
    // perform the default action of the signal.
    if (ka->sa_handler == SIG_DFL) {
      // For other processes, since the default action depends on the
      // type of signal, the function executes a switch statement based
      // on the value of signr.
      switch (signr) {
          // The signals whose default action is "ignore" are easily handled:
        case SIGCONT:
        case SIGCHLD:
        case SIGURG:
        case SIGWINCH:
          continue;
          // The signals whose default action is "stop" may stop the
          // current process. To do this, do_signal( ) sets the state
          // of current to TASK_STOPPED and then invokes the schedule( )
          // function (see Section 11.2.2).
          // The difference between SIGSTOP and the other signals is:
          //  SIGSTOP always stops the process;
          //  The other signals stop the process only if it is not
          //  in an "orphaned process group."
        case SIGTSTP:
        case SIGTTIN:
        case SIGTTOU:
          if (is_orphaned_pgrp(current->pgid))
            continue;

        case SIGSTOP:
          __unlock_task_sighand(current);
          __do_signal_stop(current, f, signr);
          __lock_task_sighand(current);

          continue;
        case SIGQUIT:
        case SIGILL:
        case SIGTRAP:

        case SIGABRT:
          sys_exit(3);

          continue;
        case SIGFPE:
        case SIGSEGV:
        case SIGBUS:
        case SIGSYS:
        case SIGXCPU:
        case SIGXFSZ:
#if 0
                if (do_coredump(signr, f))
                    exit_code |= 0x80;
#endif
        default:
#if 0
                current->flags |= PF_SIGNALED;
#endif
          sys_exit(exit_code);
          __unlock_task_sighand(current);
          return 1;
      }
    }
    if (__handle_signal(signr, &info, ka, f) == 1) {
      __unlock_task_sighand(current);
      return 1;
    }
    dprintf("Failed to handle signal.\n");
  }
  __unlock_task_sighand(current);
  return 0;
}

int signals_init() {
  // if ((sigqueue_cachep = KMEM_CREATE(sigqueue_t)) == NULL) {
  //   dprintf("Failed to allocate cache for signals.\n");
  //   return 0;
  // }

  list_head_init(&stopped_queue.task_list);
  return 1;
}

/// @brief Checks for some types of signals that might nullify other pending
/// signals for the destination thread group
/// @param sig Signal number
/// @param info siginfo struct of the signal
/// @param p Target process of the signal
void handle_stop_signal(int sig, siginfo_t *info, struct task_struct *p) {
  // remove the SIGCONT signal from the shared
  // pending signal queue p->signal->shared_pending and from the private
  // queues of all members of the thread group.
  if (sig == SIGSTOP || sig == SIGTSTP || sig == SIGTTIN || sig == SIGTTOU) {
    // TODO: shared and thread group

    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGCONT);

    __rm_from_queue(&mask, &p->pending);
  }

  // remove any SIGSTOP, SIGTSTP, SIGTTIN, and SIGTTOU signal from the shared pending signal queue p->signal->shared_pending;
  // then, removes the same signals from the private pending signal queues of the processes belonging to the thread
  // group, and awakens them
  if (sig == SIGCONT) {
    sigset_t mask;
    sigemptyset(&mask);

    sigaddset(&mask, SIGSTOP);
    sigaddset(&mask, SIGTSTP);
    sigaddset(&mask, SIGTTIN);
    sigaddset(&mask, SIGTTOU);

    __rm_from_queue(&mask, &p->pending);

    struct list_head *it, *tmp;
    list_for_each_safe(it, tmp, &stopped_queue.task_list) {
      struct wait_queue_entry_t *entry =
        list_entry(it, struct wait_queue_entry_t, task_list);

      // Select only the waiting entry for the timer task pid
      task_struct *task = entry->task;
      if (task->pid == p->pid) {
        // Executed entry's wakeup test function
        int res = entry->func(entry, 0, 0);
        if (res == 1) {
          // Removes entry from list and memory
          remove_wait_queue(&stopped_queue, entry);
          kfree(entry);

          dprintf("Process (pid: %d) restored from stop\n", p->pid);
        }
        break;
      }
    }
  }
}

/// @brief Send siginfo to target process
/// @param sig Signal number
/// @param info siginfo struct of the signal to be sent
/// @param p Target process where the signal will be sent
/// @return Returns 0 if there is no error, otherwise returns an error code
int __send_sig_info(int sig, siginfo_t *info, struct task_struct *p) {
  if (sig < 0 || sig > NSIG)
    return -EINVAL;

  // If the signal is being sent by a User Mode process,
  // it checks whether the operation is allowed.
  if (info->si_code == SI_USER) {
    // TODO
  }

  // If the sig parameter has the value 0,
  // it returns immediately without generating any signal
  if (!sig)
    return 0;

  __lock_task_sighand(p);

  // Checks for some types of signals that might nullify other pending
  // signals for the destination thread group
  handle_stop_signal(sig, info, p);

#if 0
    // Checks whether the signal is non-real-time and another occurrence of the same
    // signal is already pending in the shared pending signal queue of the thread group
    if (sig < 32 && sigismember(&p->signal->shared_pending.signal,sig))
        return 0;
#endif

  __unlock_task_sighand(p);
  __send_signal(sig, info, p);
  return 0;
}

int sys_kill(pid_t pid, int sig) {
  dprintf("sys_kill(%d, %d)\n", pid, sig);
  struct task_struct *current = scheduler_get_running_process(pid);
  // Check the task associated with the pid.
  if (!current)
    return -ESRCH;
  // Check the signal that we want to send.
  if ((sig < 0) || (sig >= NSIG))
    return -EINVAL;
  siginfo_t info;
  info.si_signo           = sig;
  info.si_code            = SI_USER;
  info.si_value.sival_int = 0;
  info.si_errno           = 0;
  info.si_pid             = current->pid;
  info.si_uid             = current->uid;
  info.si_addr            = NULL;
  info.si_status          = 0;
  info.si_band            = 0;
  return __send_sig_info(sig, &info, current);
}

sighandler_t sys_signal(int signum, sighandler_t handler,
                        uint32_t sigreturn_addr) {
  dprintf("sys_signal(%d, %p, %p)\n", signum, handler, sigreturn_addr);
  // Check the signal that we want to send.
  if ((signum < 0) || (signum >= NSIG)) {
    dprintf("sys_signal(%d, %p): Wrong signal number!\n", signum, handler);
    return SIG_ERR;
  }
  // The do_signal() function is usually only invoked when the CPU is going
  // to return in User Mode.
  struct task_struct *current = scheduler_get_current_process();
  assert(current && "There is no running process.");
  // Skip the `init` process, always.
  if (current->pid == 1) {
    dprintf("sys_signal(%d, %p): Cannot signal init!\n", signum, handler);
    return SIG_ERR;
  }
  // Create a new signal action.
  sigaction_t new_sigaction;
  // Set the handler.
  new_sigaction.sa_handler = handler;
  // Set the handler.
  new_sigaction.sa_flags = SA_RESETHAND | SA_NODEFER;
  // Reset the set for the signal action.
  sigemptyset(&new_sigaction.sa_mask);
  // Lock the signal handling for the given task.
  __lock_task_sighand(current);
  // Set the address of the sigreturn.
  current->sigreturn_addr = sigreturn_addr;
  // Get the old sigaction.
  sigaction_t *old_sigaction = &current->sighand.action[signum - 1];
  dprintf("sys_signal(%d, %p): Signal action ptr %p\n", signum, handler,
          old_sigaction);
  dprintf("sys_signal(%d, %p): Old signal handler %p\n", signum, handler,
          old_sigaction->sa_handler);
  // Get the old handler (to return).
  sighandler_t old_handler = current->sighand.action[signum - 1].sa_handler;
  // Set the new action.
  memcpy(old_sigaction, &new_sigaction, sizeof(sigaction_t));
  // Unlock the signal handling for the given task.
  __unlock_task_sighand(current);
  // Return the old sighandler.
  return old_handler;
}

int sys_sigaction(int signum, const sigaction_t *act, sigaction_t *oldact,
                  uint32_t sigreturn_addr) {
  dprintf("sys_sigaction(%d, %p, %p, %p)\n", signum, act, oldact,
          sigreturn_addr);
  // Check the signal that we want to send.
  if ((signum < 0) || (signum >= NSIG)) {
    dprintf("sys_sigaction(%d, %p, %p): Wrong signal number!\n", signum, act,
            oldact);
    return -EINVAL;
  }
  // The do_signal() function is usually only invoked when the CPU is going
  // to return in User Mode.
  struct task_struct *current = scheduler_get_current_process();
  assert(current && "There is no running process.");
  // Skip the `init` process, always.
  if (current->pid == 1) {
    dprintf("sys_sigaction(%d, %p, %p): Cannot set signal for init!\n", signum,
            act, oldact);
    return -EINVAL;
  }
  // Lock the signal handling for the given task.
  __lock_task_sighand(current);
  // Set the address of the sigreturn.
  current->sigreturn_addr = sigreturn_addr;
  // Get a pointer to the entry in the sighand.action array.
  sigaction_t *current_sigaction = &current->sighand.action[signum - 1];
  dprintf("sys_sigaction(%d, %p, %p): : Signal old action ptr %p\n", signum,
          act, oldact, current_sigaction);
  // If requested, get the old sigaction.
  if (oldact) {
    memcpy(oldact, current_sigaction, sizeof(sigaction_t));
  }
  // Set the new action.
  memcpy(current_sigaction, act, sizeof(sigaction_t));
  // Unlock the signal handling for the given task.
  __unlock_task_sighand(current);
  // Return the old sighandler.
  return 0;
}

int sys_sigprocmask(int how, const sigset_t *set, sigset_t *oldset) {
  dprintf("sys_sigprocmask(%d, %p, %p)\n", how, set, oldset);
  if (!set && !oldset) {
    return -EFAULT;
  }
  if ((how < SIG_BLOCK) || (how > SIG_SETMASK)) {
    return -EINVAL;
  }
  // The do_signal() function is usually only invoked when the CPU is going
  // to return in User Mode.
  struct task_struct *current = scheduler_get_current_process();
  assert(current && "There is no running process.");
  // Skip the `init` process, always.
  if (current->pid == 1) {
    dprintf("sys_sigprocmask(%d, %p, %p): Cannot set signal for init!\n", how,
            set, oldset);
    return -EINVAL;
  }
  // If `oldset` is not, return the old set.
  if (oldset) {
    oldset->sig[0] = current->blocked.sig[0];
    oldset->sig[1] = current->blocked.sig[1];
  }
  // Set the new signal mask.
  if (set) {
    if (how == SIG_BLOCK) {
      // The set of blocked signals is the union of the current set
      // and the set argument.
      current->blocked.sig[0] |= set->sig[0];
      current->blocked.sig[1] |= set->sig[1];
    } else if (how == SIG_UNBLOCK) {
      // The signals in set are removed from the current set of
      // blocked signals.  It is permissible to attempt to unblock
      // a signal which is not blocked.
      current->blocked.sig[0] &= ~(set->sig[0]);
      current->blocked.sig[1] &= ~(set->sig[1]);
    } else if (how == SIG_SETMASK) {
      // The set of blocked signals is set to the argument set.
      current->blocked.sig[0] = set->sig[0];
      current->blocked.sig[1] = set->sig[1];
    }
  }
  return 0;
}

const char *strsignal(int sig) {
  if ((sig >= SIGHUP) && (sig < NSIG))
    return sys_siglist[sig - 1];
  return NULL;
}

int sigemptyset(sigset_t *set) {
  if (set) {
    set->sig[0] = 0;
    return 0;
  }
  return -1;
}

int sigfillset(sigset_t *set) {
  if (set) {
    set->sig[0] = ~0UL;
    return 0;
  }
  return -1;
}

int sigaddset(sigset_t *set, int signum) {
  if (set && ((signum))) {
    bit_set_assign(set->sig[(signum - 1) / 32], ((signum - 1) % 32));
    return 0;
  }
  return -1;
}

int sigdelset(sigset_t *set, int signum) {
  if (set) {
    bit_clear_assign(set->sig[(signum - 1) / 32], ((signum - 1) % 32));
    return 0;
  }
  return -1;
}

int sigismember(sigset_t *set, int signum) {
  if (set)
    return bit_check(set->sig[(signum - 1) / 32], (signum - 1) % 32);
  return -1;
}
