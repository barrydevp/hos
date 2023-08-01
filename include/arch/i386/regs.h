#pragma once

#include <kernel/types.h>

/* Struct which aggregates many registers */
struct __attribute__((packed)) interrupt_regs {
	uint32_t gs, fs, es, ds; // Data segment selector
	uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax; // Pushed by pusha.
	uint32_t int_no,
		err_code; // Interrupt number and error code (if applicable)
	uint32_t eip, cs, eflags, useresp,
		ss; // Pushed by the processor automatically.
};
