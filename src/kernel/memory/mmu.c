#include <kernel/memory/mmu.h>
#include <kernel/memory/vmm.h>

void mmu_init(boot_info_t *boot_info) {
  kheap_init(boot_info);
}
