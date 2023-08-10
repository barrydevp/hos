#include <arch/i386/irq.h>
#include <arch/i386/ports.h>
#include <arch/i386/pic.h>
#include <kernel/string.h>
#include <kernel/printf.h>

extern void idt_load(uint32_t);

static struct idt_descriptor idt[I86_MAX_INTERRUPTS];
static struct idtr idtr;

// array of function pointers to handle each interrupt
static I86_IRQ_HANDLER isr_handlers[I86_MAX_INTERRUPTS];

void isr_install_handler(uint32_t index, I86_IRQ_HANDLER handler) {
  isr_handlers[index] = handler;
}

/**
 * @brief Initialize a gate, since there's some address swizzling involved...
 */
void idt_set_gate(uint32_t i, I86_IVT isr, uint16_t sel, uint8_t flags) {
  if (i > I86_MAX_INTERRUPTS)
    return;

  uint32_t base = (uint32_t)isr;

  idt[i].base_lo = base & 0xffff;
  idt[i].base_hi = (base >> 16) & 0xffff;
  idt[i].flags = flags;
  idt[i].sel = sel;
  idt[i].reserved = 0;
}

static void idt_default_handler(struct interrupt_regs *regs) {
  // disable_interrupts();
  //
  // uint8_t int_no = regs->int_no & 0xff;
  //
  // err("IDT: unhandled exception %d", int_no);
  //
  // halt();
  // int a = 1;
}

void idt_init() {
  idtr.limit = sizeof(idt);
  idtr.base = (uint32_t)idt;

  memset(idt, 0, sizeof(idt));

  // for (int i = 0; i < 256; ++i) {
  // 	setvect(i, idt_default_handler);
  // 	INIT_LIST_HEAD(&interrupt_handlers[i]);
  // }

  /** ISRs
   *  0x08 = address of Code Segment in GDT
   *  0x8E = I86_IDT_DESC_PRESENT | I86_IDT_DESC_BIT32
   *  because it's too long for one line so I use 0x8E instead */
  idt_set_gate(0, isr0, 0x08, 0x8E);
  idt_set_gate(1, isr1, 0x08, 0x8E);
  idt_set_gate(2, isr2, 0x08, 0x8E);
  idt_set_gate(3, isr3, 0x08, 0x8E);
  idt_set_gate(4, isr4, 0x08, 0x8E);
  idt_set_gate(5, isr5, 0x08, 0x8E);
  idt_set_gate(6, isr6, 0x08, 0x8E);
  idt_set_gate(7, isr7, 0x08, 0x8E);
  idt_set_gate(8, isr8, 0x08, 0x8E);
  idt_set_gate(9, isr9, 0x08, 0x8E);
  idt_set_gate(10, isr10, 0x08, 0x8E);
  idt_set_gate(11, isr11, 0x08, 0x8E);
  idt_set_gate(12, isr12, 0x08, 0x8E);
  idt_set_gate(13, isr13, 0x08, 0x8E);
  idt_set_gate(14, isr14, 0x08, 0x8E);
  idt_set_gate(15, isr15, 0x08, 0x8E);
  idt_set_gate(16, isr16, 0x08, 0x8E);
  idt_set_gate(17, isr17, 0x08, 0x8E);
  idt_set_gate(18, isr18, 0x08, 0x8E);
  idt_set_gate(19, isr19, 0x08, 0x8E);
  idt_set_gate(20, isr20, 0x08, 0x8E);
  idt_set_gate(21, isr21, 0x08, 0x8E);
  idt_set_gate(22, isr22, 0x08, 0x8E);
  idt_set_gate(23, isr23, 0x08, 0x8E);
  idt_set_gate(24, isr24, 0x08, 0x8E);
  idt_set_gate(25, isr25, 0x08, 0x8E);
  idt_set_gate(26, isr26, 0x08, 0x8E);
  idt_set_gate(27, isr27, 0x08, 0x8E);
  idt_set_gate(28, isr28, 0x08, 0x8E);
  idt_set_gate(29, isr29, 0x08, 0x8E);
  idt_set_gate(30, isr30, 0x08, 0x8E);
  idt_set_gate(31, isr31, 0x08, 0x8E);

  /** IRQs */
  idt_set_gate(32, irq0, 0x08, 0x8E);
  idt_set_gate(33, irq1, 0x08, 0x8E);
  idt_set_gate(34, irq2, 0x08, 0x8E);
  idt_set_gate(35, irq3, 0x08, 0x8E);
  idt_set_gate(36, irq4, 0x08, 0x8E);
  idt_set_gate(37, irq5, 0x08, 0x8E);
  idt_set_gate(38, irq6, 0x08, 0x8E);
  idt_set_gate(39, irq7, 0x08, 0x8E);
  idt_set_gate(40, irq8, 0x08, 0x8E);
  idt_set_gate(41, irq9, 0x08, 0x8E);
  idt_set_gate(42, irq10, 0x08, 0x8E);
  idt_set_gate(43, irq11, 0x08, 0x8E);
  idt_set_gate(44, irq12, 0x08, 0x8E);
  idt_set_gate(45, irq13, 0x08, 0x8E);
  idt_set_gate(46, irq14, 0x08, 0x8E);
  idt_set_gate(47, irq15, 0x08, 0x8E);

  /** Flush the idtr register */
  idt_load((uint32_t)&idtr);
}

static void panic(const char *desc, struct interrupt_regs *r,
                  uint32_t faulting_address) {
  /* Stop all other cores */
  // arch_fatal_prepare();
  //
  /* Print the description, current process, cause */
  // dprintf("\033[31mPanic!\033[0m %s pid=%d (%s) at %#zx\n", desc,
  //         this_core->current_process ? (int)this_core->current_process->id : 0,
  //         this_core->current_process ? this_core->current_process->name :
  //                                      "kernel",
  //         faulting_address);

  /* Dump register state */
  // dprintf(
  //   "Registers at interrupt:\n"
  //   "  $eip=0x%016lx\n"
  //   "  $esi=0x%016lx,$edi=0x%016lx,$ebp=0x%016lx,$esp=0x%016lx\n"
  //   "  $eax=0x%016lx,$ebx=0x%016lx,$ecx=0x%016lx,$edx=0x%016lx\n"
  //   "  $gs= 0x%016lx,$fs= 0x%016lx,$es=0x%016lx,$ds=0x%016lx\n"
  //   "  cs=0x%016lx  ss=0x%016lx eflags=0x%016lx int=0x%02lx err=0x%02lx\n",
  //   r->eip, r->esi, r->edi, r->ebp, r->esp, r->eax, r->ebx, r->ecx, r->edx,
  //   r->gs, r->fs, r->es, r->ds, r->cs, r->ss, r->eflags, r->int_no,
  //   r->err_code);

  // /* Dump GS segment register information */
  // uint32_t gs_base_low, gs_base_high;
  // asm volatile("rdmsr"
  //              : "=a"(gs_base_low), "=d"(gs_base_high)
  //              : "c"(0xc0000101));
  // uint32_t kgs_base_low, kgs_base_high;
  // asm volatile("rdmsr"
  //              : "=a"(kgs_base_low), "=d"(kgs_base_high)
  //              : "c"(0xc0000102));
  // dprintf("  gs=0x%08x%08x kgs=0x%08x%08x\n", gs_base_high, gs_base_low,
  //         kgs_base_high, kgs_base_low);
  //
  // /* Walk the call stack from before the interrupt */
  // safe_dump_traceback(r);
  //
  // /* Stop this core */
  // arch_fatal();

  dprintf("Panic! (");
  dprintf(desc);
  dprintf(")\n");
}

/**
 * @brief Double fault should always panic.
 */
static void _double_fault(struct interrupt_regs *r) {
  panic("Double fault", r, 0);
}

/**
 * @brief GPF handler.
 *
 * Mostly this is separated from other exceptions because
 * GPF should cause SIGSEGV rather than SIGILL? I think?
 *
 * @param r Interrupt register context
 */
static void _general_protection_fault(struct interrupt_regs *r) {
  /* Were we in the kernel? */
  // if (!this_core->current_process || r->cs == 0x08) {
  /* Then that's a panic. */
  panic("GPF in kernel", r, 0);
  // }

  /* Else, segfault the current process. */
  // send_signal(this_core->current_process->id, SIGSEGV, 1);
}

/**
 * @brief Page fault handler.
 *
 * Handles magic return addresses, stack expansions, maybe
 * later will handle COW or mmap'd filed... otherwise,
 * mostly segfaults.
 *
 * @param r Interrupt register context
 */
static void _page_fault(struct interrupt_regs *r) {
  /* Obtain the "cause" address */
  uintptr_t faulting_address;
  asm volatile("mov %%cr2, %0" : "=r"(faulting_address));

  // /* 8DEADBEEFh is the magic ret-from-sig address. */
  // if (faulting_address == 0x8DEADBEEF) {
  //   return_from_signal_handler(r);
  //   return;
  // }
  //
  // if ((r->err_code & 3) == 3) {
  //   /* This is probably a COW page? */
  //   extern int mmu_copy_on_write(uintptr_t address);
  //   if (!mmu_copy_on_write(faulting_address))
  //     return;
  // }
  //
  // /* Was this a kernel page fault? Those are always a panic. */
  // if (!this_core->current_process || r->cs == 0x08) {
  //   panic("Page fault in kernel", r, faulting_address);
  panic("Page fault in kernel", r, faulting_address);
  // }
  //
  // /* Page was present but not writable */
  //
  // /* Quietly map more stack if it was a viable stack address. */
  // if (faulting_address < 0x800000000000 && faulting_address > 0x700000000000) {
  //   if (map_more_stack(faulting_address & 0xFFFFffffFFFFf000))
  //     return;
  // }
  //
  // /* Otherwise, segfault the current process. */
  // send_signal(this_core->current_process->id, SIGSEGV, 1);
}

/**
 * @brief Handle an exception interrupt.
 *
 * @param r           Interrupt register context
 * @param description Textual description of the exception, for panic messages.
 */
static void _exception(struct interrupt_regs *r, const char *description) {
  /* If we were in kernel space, this is a panic */
  // if (!this_core->current_process || r->cs == 0x08) {
  // 	panic(description, r, r->int_no);
  // }
  if (r->cs == 0x08) {
    panic(description, r, r->int_no);
  }
  /* Otherwise, these interrupts should trigger SIGILL */
  // send_signal(this_core->current_process->id, SIGILL, 1);
}

/**
 * @brief Handle an installable interrupt. This handles PIC IRQs
 *        that need to be acknowledged.
 *
 * @param r    Interrupt register context
 * @param irq  Translated IRQ number
 */
static void _handle_irq(struct interrupt_regs *r, uint32_t irq) {
  // for (size_t i = 0; i < IRQ_CHAIN_DEPTH; i++) {
  //   I86_IRQ_HANDLER handler = irq_routines[i * IRQ_CHAIN_SIZE + irq];
  //   if (!handler)
  //     break;
  //   if (handler(r))
  //     return;
  // }

  I86_IRQ_HANDLER handler = isr_handlers[irq];
  if (handler(r) == IRQ_CONTINUE)
    return;

  /* Unhandled */
  irq_ack(irq);
}

// clang-format off
#define EXC(i,n) case i: _exception(r, n); break;
#define IRQ(i) case i: _handle_irq(r,i-32); break;
// clang-format on

void isr_handler_inner(struct interrupt_regs *r) {
  // clang-format off
  switch (r->int_no) {
    EXC(0, "divide-by-zero");
    case 1:
      // return _debug_int(r);
    /* NMI doesn't reach here, we use it as a panic signal. */
    EXC(3, "breakpoint"); /* TODO: This should map to a ptrace event */
    EXC(4, "overflow");
    EXC(5, "bound range exceeded");
    EXC(6, "invalid opcode");
    EXC(7, "device not available");
    case 8:
      _double_fault(r);
      break;
    /* 9 is a legacy exception that shouldn't happen */
    EXC(10, "invalid TSS");
    EXC(11, "segment not present");
    EXC(12, "stack-segment fault");
    case 13:
      _general_protection_fault(r);
      break;
    case 14:
      _page_fault(r);
      break;
    /* 15 is reserved */
    EXC(16, "floating point exception");
    EXC(17, "alignment check");
    EXC(18, "machine check");
    EXC(19, "SIMD floating-point exception");
    EXC(20, "virtualization exception");
    EXC(21, "control protection exception");
    /* 22 through 27 are reserved */
    EXC(28, "hypervisor injection exception");
    EXC(29, "VMM communication exception");
    EXC(30, "security exception");
    /* 31 is reserved */

    /* 16 IRQs that go to the general IRQ chain */
    IRQ(32);
    IRQ(33);
    IRQ(34);
    IRQ(35);
    IRQ(36);
    IRQ(37);
    IRQ(38);
    case 39:
      break; /* Except the spurious IRQ, just ignore that */
    IRQ(40);
    IRQ(41);
    IRQ(42);
    IRQ(43);
    IRQ(44);
    IRQ(45);
    IRQ(46);
    IRQ(47);

    /* Local interrupts that make it here. */
    case 123:
      // return _local_timer(r);
    case 127:
      // return _syscall_entrypoint(r);

      /* Other interrupts that don't make it here:
     *   124: TLB shootdown, we just reload CR3 in the handler.
     *   125: Fatal signal, jumps straight to a cli/hlt loop, though I
     * think this just yields an NMI instead? 126: Quiet wakeup, do we
     * even use this anymore?
     */

    default:
      panic("Unexpected interrupt", r, 0);
  }
  // clang-format on

  // if (this_core->current_process == this_core->kernel_idle_task &&
  //     process_queue && process_queue->head) {
  // 	/* If this is kidle and we got here, instead of finishing the interrupt
  //         * we can just switch task and there will probably be something else
  //         * to run that was awoken by the interrupt. */
  // 	switch_next();
  // }

  // return r;
}

void isr_handler(struct interrupt_regs *r) {
  isr_handler_inner(r);
}

void irq_ack(uint32_t irq_number) {
  if (irq_number >= 8)
    outportb(PIC2_COMMAND, PIC_EOI);
  outportb(PIC1_COMMAND, PIC_EOI);
}
