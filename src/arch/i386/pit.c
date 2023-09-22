#include <arch/i386/pit.h>
#include <arch/i386/irq.h>
#include <arch/i386/ports.h>

volatile uint64_t ticks = 0; // in milliseconds

/**
 * @brief Set the phase of the PIT in Hz.
 *
 * @param hz Ticks per second.
 */
static void pit_set_timer_phase(long hz) {
  long divisor = PIT_SCALE / hz;

  outportb(PIT_CONTROL, PIT_SET);
  outportb(PIT_A, divisor & PIT_MASK);
  outportb(PIT_A, (divisor >> 8) & PIT_MASK);
}

/**
 * @brief Interrupt handler for the PIT.
 */
static int32_t pit_interrupt(pt_regs *r) {
  // extern void arch_update_clock(void);
  // arch_update_clock();

  ticks++;

  irq_ack(r->int_no);

  // switch_task(1);
  // asm volatile(".global _ret_from_preempt_source\n"
  //              "_ret_from_preempt_source:");

  return IRQ_STOP;
}

/**
 * @brief Install an interrupt handler for, and turn on, the PIT.
 */
void pit_init(void) {
  /* register ISR for PIT IRQ */
  isr_install_handler(TIMER_IRQ, pit_interrupt);

  /* Enable PIT */
  pit_set_timer_phase(100);
}
