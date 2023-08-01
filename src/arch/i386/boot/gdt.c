#include <arch/i386/boot/gdt.h>
#include <kernel/string.h>

static struct gdt_descriptor gdt[MAX_DESCRIPTORS]; // 3 descriptors

static struct gdtr gdtr; // one gdt register

struct gdt_descriptor *get_gdt_entry(uint32_t index) {
  if (index > MAX_DESCRIPTORS) {
    return 0;
  }

  return &gdt[index];
}

void gdt_set_descriptor(uint32_t i, uint64_t base, uint64_t limit,
                        uint8_t access, uint8_t grand) {
  if (i > MAX_DESCRIPTORS)
    return;

  //! null out the descriptor
  memset((void *)&gdt[i], 0, sizeof(struct gdt_descriptor));

  //! set limit and base addresses
  gdt[i].base_lo = (uint16_t)(base & 0xffff);
  gdt[i].base_mi = (uint8_t)((base >> 16) & 0xff);
  gdt[i].base_hi = (uint8_t)((base >> 24) & 0xff);
  gdt[i].limit = (uint16_t)(limit & 0xffff);

  //! set flags and grandularity bytes
  gdt[i].flags = access;
  gdt[i].grand = (uint8_t)((limit >> 16) & 0x0f);
  gdt[i].grand |= grand & 0xf0;
}

void gdt_init() {
  gdtr.limit = (sizeof(struct gdt_descriptor) * MAX_DESCRIPTORS) - 1;
  gdtr.base = (uint32_t)&gdt[0];

  //! set null descriptor
  gdt_set_descriptor(0, 0, 0, 0, 0);

  //! set default code descriptor
  gdt_set_descriptor(1, 0, 0xffffffff,
                     I86_GDT_DESC_READWRITE | I86_GDT_DESC_EXEC_CODE |
                       I86_GDT_DESC_CODEDATA | I86_GDT_DESC_MEMORY,
                     I86_GDT_GRAND_4K | I86_GDT_GRAND_32BIT |
                       I86_GDT_GRAND_LIMITHI_MASK);

  //! set default data descriptor
  gdt_set_descriptor(
    2, 0, 0xffffffff,
    I86_GDT_DESC_READWRITE | I86_GDT_DESC_CODEDATA | I86_GDT_DESC_MEMORY,
    I86_GDT_GRAND_4K | I86_GDT_GRAND_32BIT | I86_GDT_GRAND_LIMITHI_MASK);

  //! set default user mode code descriptor
  gdt_set_descriptor(
    3, 0, 0xffffffff,
    I86_GDT_DESC_READWRITE | I86_GDT_DESC_EXEC_CODE | I86_GDT_DESC_CODEDATA |
      I86_GDT_DESC_MEMORY | I86_GDT_DESC_DPL,
    I86_GDT_GRAND_4K | I86_GDT_GRAND_32BIT | I86_GDT_GRAND_LIMITHI_MASK);

  //! set default user mode data descriptor
  gdt_set_descriptor(4, 0, 0xffffffff,
                     I86_GDT_DESC_READWRITE | I86_GDT_DESC_CODEDATA |
                       I86_GDT_DESC_MEMORY | I86_GDT_DESC_DPL,
                     I86_GDT_GRAND_4K | I86_GDT_GRAND_32BIT |
                       I86_GDT_GRAND_LIMITHI_MASK);

  gdt_load((uint32_t)&gdtr);
}
