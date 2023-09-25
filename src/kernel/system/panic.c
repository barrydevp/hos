#include <kernel/arch.h>
#include <kernel/system/panic.h>
#include <kernel/printf.h>

void kernel_panic(const char *msg) {
  dprintf("\nPANIC:\n%s\n\nWelcome to Kernel Debugging Land...\n\n", msg);
  dprintf("\n");
  arch_fatal();
}
