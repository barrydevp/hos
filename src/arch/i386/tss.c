/// Change the header.
#define __DEBUG_HEADER__ "[TSS   ]"
/// Set the log level.
#define __DEBUG_LEVEL__ LOGLEVEL_NOTICE

#include <arch/i386/tss.h>
#include <arch/i386/gdt.h>

#include <kernel/string.h>

static tss_entry_t kernel_tss;

void tss_init(uint8_t idx, uint32_t ss0) {
  uint32_t base = (uint32_t)&kernel_tss;
  uint32_t limit = base + sizeof(tss_entry_t);

  // Add the TSS descriptor to the GDT.
  // Kernel tss, access(E9 = 1 11 0 1 0 0 1)
  //    1   present
  //    11  ring 3 (kernel)
  //    0   always 0 when dealing with system segments
  //    1   execution
  //    0   can not be executed by ring lower or equal to DPL,
  //    0   not readable
  //    1   access bit, always 0, cpu set this to 1 when accessing this sector
  gdt_set_descriptor(idx, base, limit,
                     I86_GDT_DESC_ACCESS | I86_GDT_DESC_EXEC_CODE |
                       I86_GDT_DESC_DPL | I86_GDT_DESC_MEMORY,
                     0x0);

  // Note that we usually set tss's esp to 0 when booting our os, however,
  // we need to set it to the real esp when we've switched to usermode
  // because the CPU needs to know what esp to use when usermode app is
  // calling a kernel function(aka system call), that's why we have a
  // function below called tss_set_stack.
  memset(&kernel_tss, 0x0, sizeof(tss_entry_t));
  kernel_tss.ss0 = ss0;
  kernel_tss.esp0 = 0x0;
  kernel_tss.cs = 0x0b;
  kernel_tss.ds = 0x13;
  kernel_tss.es = 0x13;
  kernel_tss.fs = 0x13;
  kernel_tss.gs = 0x13;
  kernel_tss.ss = 0x13;
  kernel_tss.iomap = sizeof(tss_entry_t);

  tss_flush();
}

void tss_set_stack(uint32_t kss, uint32_t kesp) {
  // Kernel data segment.
  kernel_tss.ss0 = kss;
  // Kernel stack address.
  kernel_tss.esp0 = kesp;
}
