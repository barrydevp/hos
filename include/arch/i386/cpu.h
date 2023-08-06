#pragma once

#include <kernel/types.h>

static inline void io_wait() {
  /* https://stackoverflow.com/questions/6793899/what-does-the-0x80-port-address-connect-to */
  /* Port 0x80 is used for 'checkpoints' during POST. */
  /* The Linux kernel seems to think it is free for use :-/ */
  asm volatile("outb %%al, $0x80" : : "a"(0));
}

//! enable all hardware interrupts
static inline void enable_interrupts() {
  asm volatile("sti");
}

//! disable all hardware interrupts
static inline void disable_interrupts() {
  asm volatile("cli");
}

static inline void halt() {
  asm volatile("hlt");
}

void cpuid(int code, uint32_t *a, uint32_t *d);
const char *get_cpu_vender();
