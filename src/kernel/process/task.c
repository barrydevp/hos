#include <kernel/process/task.h>
#include <kernel/process/scheduler.h>
#include <kernel/process/wait.h>
#include <kernel/process/prio.h>
#include <kernel/process/elf.h>

#include <arch/i386/timer.h>

#include <kernel/kernel.h>
#include <kernel/memory/mmu.h>
#include <kernel/system/panic.h>
#include <kernel/fs/vfs.h>
#include <kernel/assert.h>
#include <kernel/libgen.h>
#include <kernel/string.h>
#include <kernel/bitops.h>
#include <kernel/errno.h>
#include <kernel/fcntl.h>
#include <kernel/printf.h>

/// Cache for creating the task structs.
// static kmem_cache_t *task_struct_cache;
/// @brief The task_struct of the init process.
static task_struct *init_proc;

/// @brief Counts the number of arguments.
/// @param args the array of arguments, it must be NULL terminated.
/// @return the number of arguments.
static inline int __count_args(char **args) {
  int argc = 0;
  while (args[argc] != NULL) ++argc;
  return argc;
}

/// @brief Counts the bytes occupied by the arguments.
/// @param args the array of arguments, it must be NULL terminated.
/// @return the bytes occupied by the arguments.
static inline int __count_args_bytes(char **args) {
  // Count the number of arguments.
  int argc = __count_args(args);
  // Count the number of characters.
  int nchar = 0;
  for (int i = 0; i < argc; i++) { nchar += strlen(args[i]) + 1; }
  return nchar + (argc + 1 /* The NULL terminator */) * sizeof(char *);
}

/// @brief Pushes the arguments on the stack.
/// @param stack pointer to the stack location.
/// @param args the list of arguments.
/// @return the final position of the stack, where the list of pushed arguments is stored.
static inline char **__push_args_on_stack(uintptr_t *stack, char *args[]) {
  // Count the number of arguments.
  int argc = __count_args(args);
  // Prepare args with space for the terminating NULL.
  char *args_location[256];
  for (int i = argc - 1; i >= 0; --i) {
    for (int j = strlen(args[i]); j >= 0; --j) {
      PUSH_VALUE_ON_STACK(*stack, args[i][j]);
    }
    args_location[i] = (char *)(*stack);
  }
  // Push terminating NULL.
  PUSH_VALUE_ON_STACK(*stack, (char *)NULL);
  // Push array of pointers to the arguments.
  for (int i = argc - 1; i >= 0; --i) {
    PUSH_VALUE_ON_STACK(*stack, args_location[i]);
  }
  return (char **)(*stack);
}

static int __reset_process(task_struct *task) {
  dprintf("__reset_process(%p `%s`)\n", task, task->name);
  // Create a new stack segment.
  task->mm = mmu_create_blank_process_image(DEFAULT_STACK_SIZE);
  if (task->mm == NULL) {
    dprintf("Failed to initialize process mm structure.\n");
    return 0;
  }

  // Save the current page directory.
  page_directory_t *cur_pgd = vmm_get_directory();
  // FIXME: Now to clear the stack a pgdir switch is made, it should be a kernel mmapping.
  vmm_switch_directory(task->mm->pgd);

  // Clean stack space.
  memset((char *)task->mm->start_stack, 0, DEFAULT_STACK_SIZE);
  // Set the base address of the stack.
  task->thread.regs.ebp =
    (uintptr_t)(task->mm->start_stack + DEFAULT_STACK_SIZE);
  // Set the top address of the stack.
  task->thread.regs.useresp = task->thread.regs.ebp;
  // Enable the interrupts.
  task->thread.regs.eflags = task->thread.regs.eflags | EFLAG_IF;

  // Restore previous pgdir
  vmm_switch_directory(cur_pgd);

  return 1;
}

static int __load_executable(const char *path, task_struct *task,
                             uint32_t *entry) {
  dprintf("__load_executable(`%s`, %p `%s`, %p)\n", path, task, task->name,
          entry);
  vfs_file_t *file = vfs_open(path, O_RDONLY, 0);
  if (file == NULL) {
    dprintf("Cannot find executable!\n");
    return 0;
  }
  // Check that the file is actually an executable before destroying the `mm`.
  if (!elf_check_file_type(file, ET_EXEC)) {
    dprintf("This is not a valid ELF executable `%s`!\n", path);
    return 0;
  }
  // FIXME: When threads will be implemented
  // they should share the mm, so the destroy_process_image must be called
  // only when all the threads are terminated. This can be accomplished by using
  // an internal counter on the mm.
  if (task->mm)
    mmu_destroy_process_image(task->mm);
  // Return code variable.
  int ret = 0;
  // Recreate the memory of the process.
  if (__reset_process(task)) {
    // Load the elf file, check if 0 is returned and print the error.
    if (!(ret = elf_load_file(task, file, entry))) {
      dprintf("Failed to load ELF file `%s`!\n", path);
    }
  }
  // Close the file.
  vfs_close(file);
  return ret;
}

static inline task_struct *__alloc_task(task_struct *source,
                                        task_struct *parent, const char *name) {
  // Create a new task_struct.
  // task_struct *proc = kmem_cache_alloc(task_struct_cache, GFP_KERNEL);
  task_struct *proc = kmalloc(sizeof(task_struct));
  // Clear the memory.
  memset(proc, 0, sizeof(task_struct));
  // Set the id of the process.
  proc->pid = scheduler_getpid();
  // Set the state of the process as running.
  proc->state = TASK_RUNNING;
  // Set the current opened file descriptors and the maximum number of file descriptors.
  if (source)
    vfs_dup_task(proc, source);
  else
    vfs_init_task(proc);
  // Set the pointer to process's parent.
  proc->parent = parent;
  // Initialize the list_head.
  list_head_init(&proc->run_list);
  // Initialize the children list_head.
  list_head_init(&proc->children);
  // Initialize the sibling list_head.
  list_head_init(&proc->sibling);
  // If we have a parent, set the sibling child relation.
  if (parent) {
    // Set the new_process as child of current.
    list_head_insert_before(&proc->sibling, &parent->children);
  }
  if (source)
    memcpy(&proc->thread, &source->thread, sizeof(thread_struct_t));
  // Set the statistics of the process.
  proc->uid                   = 0;
  proc->gid                   = 0;
  proc->sid                   = 0;
  proc->pgid                  = 0;
  proc->se.prio               = DEFAULT_PRIO;
  proc->se.start_runtime      = timer_get_ticks();
  proc->se.exec_start         = timer_get_ticks();
  proc->se.exec_runtime       = 0;
  proc->se.sum_exec_runtime   = 0;
  proc->se.vruntime           = 0;
  proc->se.period             = 0;
  proc->se.deadline           = 0;
  proc->se.arrivaltime        = timer_get_ticks();
  proc->se.executed           = false;
  proc->se.is_periodic        = false;
  proc->se.is_under_analysis  = false;
  proc->se.next_period        = 0;
  proc->se.worst_case_exec    = 0;
  proc->se.utilization_factor = 0;
  // Initialize the exit code of the process.
  proc->exit_code = 0;
  // Copy the name.
  if (name)
    strcpy(proc->name, name);
  // Do not touch the task's segments.
  proc->mm = NULL;
  // Initialize the error number.
  proc->error_no = 0;
  // Initialize the current working directory.
  if (source)
    strcpy(proc->cwd, source->cwd);
  else
    strcpy(proc->cwd, "/");
  // Clear the signal handler.
  memset(&proc->sighand, 0x00, sizeof(sighand_t));
  spinlock_init(&proc->sighand.siglock);
  atomic_set(&proc->sighand.count, 0);
  for (int i = 0; i < NSIG; ++i) {
    proc->sighand.action[i].sa_handler = SIG_DFL;
    sigemptyset(&proc->sighand.action[i].sa_mask);
    proc->sighand.action[i].sa_flags = 0;
  }
  // Clear the masks.
  sigemptyset(&proc->blocked);
  sigemptyset(&proc->real_blocked);
  sigemptyset(&proc->saved_sigmask);
  // Initialzie the data structure storing the pending signals.
  list_head_init(&proc->pending.list);
  sigemptyset(&proc->pending.signal);

  // Initalize real_timer for intervals
  proc->real_timer = NULL;

  // Set the default terminal options.
  proc->termios =
    (termios_t){ .c_cflag = 0,
                 .c_lflag = (ICANON | ECHO | ECHOE | ECHOK | ECHONL | ISIG),
                 .c_oflag = 0,
                 .c_iflag = 0 };
  // Initialize the ringbuffer.
  fs_rb_scancode_init(&proc->keyboard_rb);

  return proc;
}

int tasking_init() {
  // if ((task_struct_cache = KMEM_CREATE(task_struct)) == NULL) {
  //   return 0;
  // }
  // return 1;

  return 1;
}

task_struct *create_task_test(const char *name) {
  dprintf("Building (%s) process...\n", name);
  // Allocate the memory for the process.
  init_proc = __alloc_task(NULL, NULL, name);

  // == INITIALIZE `/proc/video` ============================================
  // Check that the fd_list is initialized.
  assert(init_proc->fd_list && "File descriptor list not initialized.");
  assert((init_proc->max_fd > 3) &&
         "File descriptor list cannot contain the standard IOs.");

  // // Create STDIN descriptor.
  // vfs_file_t *stdin = vfs_open("/proc/video", O_RDONLY, 0);
  // stdin->count++;
  // init_proc->fd_list[STDIN_FILENO].file_struct = stdin;
  // init_proc->fd_list[STDIN_FILENO].flags_mask  = O_RDONLY;
  // dprintf("`/proc/video` stdin  : %p\n", stdin);

  // // Create STDOUT descriptor.
  // vfs_file_t *stdout = vfs_open("/proc/video", O_WRONLY, 0);
  // stdout->count++;
  // init_proc->fd_list[STDOUT_FILENO].file_struct = stdout;
  // init_proc->fd_list[STDOUT_FILENO].flags_mask  = O_WRONLY;
  // dprintf("`/proc/video` stdout : %p\n", stdout);

  // // Create STDERR descriptor.
  // vfs_file_t *stderr = vfs_open("/proc/video", O_WRONLY, 0);
  // stderr->count++;
  // init_proc->fd_list[STDERR_FILENO].file_struct = stderr;
  // init_proc->fd_list[STDERR_FILENO].flags_mask  = O_WRONLY;
  // dprintf("`/proc/video` stderr : %p\n", stderr);
  // ------------------------------------------------------------------------

  // == INITIALIZE TASK MEMORY ==============================================
  // Load the executable.
  EXTLD(base_bin_hello)
  elf_header_t *elf_hdr = (elf_header_t *)LDVAR(base_bin_hello);
  if (!__reset_process(init_proc) || !(elf_load_exec0(elf_hdr, init_proc))) {
    dprintf("Entry for init: %d\n", init_proc->thread.regs.eip);
    kernel_panic("Init not valid (%d)!");
  }
  init_proc->thread.regs.eip = elf_hdr->entry;
  // ------------------------------------------------------------------------

  // == INITIALIZE PROGRAM ARGUMENTS ========================================
  // Save the current page directory.
  page_directory_t *cur_pdir = vmm_get_directory();
  // Switch to init page directory.
  vmm_switch_directory(init_proc->mm->pgd);

  // Prepare argv and envp for the init process.
  char **argv_ptr, **envp_ptr;
  int argc            = 1;
  static char *argv[] = { "base/bin/hello", (char *)NULL };
  static char *envp[] = { (char *)NULL };
  // Save where the arguments start.
  init_proc->mm->arg_start = init_proc->thread.regs.useresp;
  // Push the arguments on the stack.
  argv_ptr = __push_args_on_stack(&init_proc->thread.regs.useresp, argv);
  // Save where the arguments end.
  init_proc->mm->arg_end = init_proc->thread.regs.useresp;
  // Save where the environmental variables start.
  init_proc->mm->env_start = init_proc->thread.regs.useresp;
  // Push the environment on the stack.
  envp_ptr = __push_args_on_stack(&init_proc->thread.regs.useresp, envp);
  // Save where the environmental variables end.
  init_proc->mm->env_end = init_proc->thread.regs.useresp;
  // Push the `main` arguments on the stack (argc, argv, envp).
  PUSH_VALUE_ON_STACK(init_proc->thread.regs.useresp, envp_ptr);
  PUSH_VALUE_ON_STACK(init_proc->thread.regs.useresp, argv_ptr);
  PUSH_VALUE_ON_STACK(init_proc->thread.regs.useresp, argc);

  // Restore previous pgdir
  vmm_switch_directory(cur_pdir);
  // ------------------------------------------------------------------------

  // Active the current process.
  scheduler_enqueue_task(init_proc);

  dprintf("Executing '%s' (pid: %d)...\n", init_proc->name, init_proc->pid);

  return init_proc;
}

task_struct *process_create_init(const char *path) {
  dprintf("Building init process...\n");
  // Allocate the memory for the process.
  init_proc = __alloc_task(NULL, NULL, "init");

  // == INITIALIZE `/proc/video` ============================================
  // Check that the fd_list is initialized.
  assert(init_proc->fd_list && "File descriptor list not initialized.");
  assert((init_proc->max_fd > 3) &&
         "File descriptor list cannot contain the standard IOs.");

  // Create STDIN descriptor.
  vfs_file_t *stdin = vfs_open("/proc/video", O_RDONLY, 0);
  stdin->count++;
  init_proc->fd_list[STDIN_FILENO].file_struct = stdin;
  init_proc->fd_list[STDIN_FILENO].flags_mask  = O_RDONLY;
  dprintf("`/proc/video` stdin  : %p\n", stdin);

  // Create STDOUT descriptor.
  vfs_file_t *stdout = vfs_open("/proc/video", O_WRONLY, 0);
  stdout->count++;
  init_proc->fd_list[STDOUT_FILENO].file_struct = stdout;
  init_proc->fd_list[STDOUT_FILENO].flags_mask  = O_WRONLY;
  dprintf("`/proc/video` stdout : %p\n", stdout);

  // Create STDERR descriptor.
  vfs_file_t *stderr = vfs_open("/proc/video", O_WRONLY, 0);
  stderr->count++;
  init_proc->fd_list[STDERR_FILENO].file_struct = stderr;
  init_proc->fd_list[STDERR_FILENO].flags_mask  = O_WRONLY;
  dprintf("`/proc/video` stderr : %p\n", stderr);
  // ------------------------------------------------------------------------

  // == INITIALIZE TASK MEMORY ==============================================
  // Load the executable.
  if (!__load_executable(path, init_proc, &init_proc->thread.regs.eip)) {
    dprintf("Entry for init: %d\n", init_proc->thread.regs.eip);
    kernel_panic("Init not valid (%d)!");
  }
  // ------------------------------------------------------------------------

  // == INITIALIZE PROGRAM ARGUMENTS ========================================
  // Save the current page directory.
  page_directory_t *cur_pdir = vmm_get_directory();
  // Switch to init page directory.
  vmm_switch_directory(init_proc->mm->pgd);

  // Prepare argv and envp for the init process.
  char **argv_ptr, **envp_ptr;
  int argc            = 1;
  static char *argv[] = { "/bin/init", (char *)NULL };
  static char *envp[] = { (char *)NULL };
  // Save where the arguments start.
  init_proc->mm->arg_start = init_proc->thread.regs.useresp;
  // Push the arguments on the stack.
  argv_ptr = __push_args_on_stack(&init_proc->thread.regs.useresp, argv);
  // Save where the arguments end.
  init_proc->mm->arg_end = init_proc->thread.regs.useresp;
  // Save where the environmental variables start.
  init_proc->mm->env_start = init_proc->thread.regs.useresp;
  // Push the environment on the stack.
  envp_ptr = __push_args_on_stack(&init_proc->thread.regs.useresp, envp);
  // Save where the environmental variables end.
  init_proc->mm->env_end = init_proc->thread.regs.useresp;
  // Push the `main` arguments on the stack (argc, argv, envp).
  PUSH_VALUE_ON_STACK(init_proc->thread.regs.useresp, envp_ptr);
  PUSH_VALUE_ON_STACK(init_proc->thread.regs.useresp, argv_ptr);
  PUSH_VALUE_ON_STACK(init_proc->thread.regs.useresp, argc);

  // Restore previous pgdir
  vmm_switch_directory(cur_pdir);
  // ------------------------------------------------------------------------

  // Active the current process.
  scheduler_enqueue_task(init_proc);

  dprintf("Executing '%s' (pid: %d)...\n", init_proc->name, init_proc->pid);

  return init_proc;
}

char *sys_getcwd(char *buf, size_t size) {
  task_struct *current = scheduler_get_current_process();
  if ((current != NULL) && (buf != NULL)) {
    strncpy(buf, current->cwd, size);
    return buf;
  }
  return (char *)-EACCES;
}

int sys_chdir(char const *path) {
  task_struct *current = scheduler_get_current_process();
  assert(current && "There is no running process.");
  if (!path)
    return -EFAULT;
  char absolute_path[PATH_MAX];
  realpath(path, absolute_path);
  // Check that the directory exists.
  vfs_file_t *dir = vfs_open(absolute_path, O_RDONLY | O_DIRECTORY, S_IXUSR);
  if (dir) {
    strcpy(current->cwd, absolute_path);
    vfs_close(dir);
    return 0;
  }
  // Return the errno value set by either VFS or the filesystem underneath.
  return -errno;
}

int sys_fchdir(int fd) {
  task_struct *current = scheduler_get_current_process();
  assert(current && "There is no running process.");
  // Check if it is a valid file descriptor.
  if ((fd < 0) || (fd >= current->max_fd))
    return -EBADF;
  // Get the file descriptor.
  vfs_file_descriptor_t *vfd = &current->fd_list[fd];
  // Check if the file descriptor file is set.
  if (vfd->file_struct == NULL)
    return -ENOENT;
  // Check that the path points to a directory.
  if (!bitmask_check(vfd->file_struct->flags, DT_DIR))
    return -ENOTDIR;
  char absolute_path[PATH_MAX];
  realpath(vfd->file_struct->name, absolute_path);
  strcpy(current->cwd, absolute_path);
  return 0;
}

pid_t sys_fork(pt_regs *f) {
  task_struct *current = scheduler_get_current_process();
  if (current == NULL)
    kernel_panic("There is no current process!");

  dprintf("Forking   '%s' (pid: %d)...\n", current->name, current->pid);

  // Update current process registers, they should be equal
  // to the ones of the child process, except for eax.
  scheduler_store_context(f, current);
  // Allocate the memory for the process.
  task_struct *proc = __alloc_task(current, current, current->name);
  // Copy the father's stack, memory, heap etc... to the child process
  proc->mm = mmu_clone_process_image(current->mm);
  // Set the eax as 0, to indicate the child process
  proc->thread.regs.eax = 0;
  // Enable the interrupts.
  proc->thread.regs.eflags = proc->thread.regs.eflags | EFLAG_IF;

  // Copy session and group id of the parent into the child
  proc->sid  = current->sid;
  proc->pgid = current->pgid;
  proc->uid  = current->uid;
  proc->gid  = current->gid;

  // Active the new process.
  scheduler_enqueue_task(proc);

  dprintf("Forked    '%s' (pid: %d, gid: %d, sid: %d, pgid: %d)...\n",
          proc->name, proc->pid, proc->gid, proc->sid, proc->pgid);

  // Return PID of child process to parent.
  return proc->pid;
}

int sys_execve(pt_regs *f) {
  // Check the current process.
  task_struct *current = scheduler_get_current_process();
  if (current == NULL)
    kernel_panic("There is no current process!");

  char **origin_argv, **saved_argv, **final_argv;
  char **origin_envp, **saved_envp, **final_envp;
  char name_buffer[NAME_MAX];

  // Get the filename.
  char *filename = (char *)f->ebx;
  if (filename == NULL) {
    dprintf("Received NULL filename.\n");
    return -1;
  }
  // Get the arguments
  origin_argv = (char **)f->ecx;
  // Get the environment.
  origin_envp = (char **)f->edx;
  // Check the argument, the environment, and that at least the name is provided.
  if (origin_argv == NULL) {
    dprintf("sys_execve failed: must provide argv.\n");
    return -1;
  }
  if (origin_argv[0] == NULL) {
    dprintf("sys_execve failed: must provide the name.\n");
    return -1;
  }
  if (origin_envp == NULL) {
    dprintf("sys_execve failed: must provide the environment.\n");
    return -1;
  }

  // Save the name of the process.
  strcpy(name_buffer, origin_argv[0]);

  // == COPY PROGRAM ARGUMENTS ==============================================
  // Copy argv and envp to kernel memory, because all the old process memory will be discarded.
  int argc       = __count_args(origin_argv);
  int argv_bytes = __count_args_bytes(origin_argv);
  int envc       = __count_args(origin_envp);
  int envp_bytes = __count_args_bytes(origin_envp);
  if ((argv_bytes < 0) || (envp_bytes < 0)) {
    dprintf(
      "Failed to count required memory to store arguments and environment (%d + %d).\n",
      argv_bytes, envp_bytes);
    return -1;
  }
  void *args_mem = kmalloc(argv_bytes + envp_bytes);
  if (!args_mem) {
    dprintf(
      "Failed to allocate memory for arguments and environment %d (%d + %d).\n",
      argv_bytes + envp_bytes, argv_bytes, envp_bytes);
    return -1;
  }
  // Copy the arguments.
  uint32_t args_mem_ptr = (uint32_t)args_mem + (argv_bytes + envp_bytes);
  saved_argv            = __push_args_on_stack(&args_mem_ptr, origin_argv);
  saved_envp            = __push_args_on_stack(&args_mem_ptr, origin_envp);
  // Check the memory pointer.
  assert(args_mem_ptr == (uint32_t)args_mem);
  // ------------------------------------------------------------------------

  // == INITIALIZE TASK MEMORY ==============================================
  if (!__load_executable(filename, current, &current->thread.regs.eip)) {
    dprintf("Failed to load executable!\n");
    // Free the temporary args memory.
    kfree(args_mem);
    return -1;
  }
  // ------------------------------------------------------------------------

  // == INITIALIZE PROGRAM ARGUMENTS ========================================
  // Save the current page directory.
  page_directory_t *cur_pgd = vmm_get_directory();

  // Change the page directory to point to the newly created process
  vmm_switch_directory(current->mm->pgd);

  // Save where the arguments start.
  current->mm->arg_start = current->thread.regs.useresp;
  // Push the arguments on the stack.
  final_argv = __push_args_on_stack(&current->thread.regs.useresp, saved_argv);
  // Save where the arguments end, and the env starts.
  current->mm->env_start = current->mm->arg_end = current->thread.regs.useresp;
  // Push the environment on the stack.
  final_envp = __push_args_on_stack(&current->thread.regs.useresp, saved_envp);
  // Save where the environmental variables end.
  current->mm->env_end = current->thread.regs.useresp;
  // Push the `main` arguments on the stack (argc, argv, envp).
  PUSH_VALUE_ON_STACK(current->thread.regs.useresp, final_envp);
  PUSH_VALUE_ON_STACK(current->thread.regs.useresp, final_argv);
  PUSH_VALUE_ON_STACK(current->thread.regs.useresp, argc);

  // Restore previous pgdir
  vmm_switch_directory(cur_pgd);
  // ------------------------------------------------------------------------

  // Change the name of the process.
  strcpy(current->name, name_buffer);

  // Free the temporary args memory.
  kfree(args_mem);

  // Perform the switch to the new process.
  scheduler_restore_context(current, f);

  dprintf("Executing '%s' (pid: %d)...\n", current->name, current->pid);
  return 0;
}
