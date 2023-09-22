#pragma once

#include <kernel/types.h>

/* Struct which aggregates many registers */
// struct __attribute__((packed)) pt_regs {
// 	uint32_t gs, fs, es, ds; // Data segment selector
// 	uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax; // Pushed by pusha.
// 	uint32_t int_no,
// 		err_code; // Interrupt number and error code (if applicable)
// 	uint32_t eip, cs, eflags, useresp,
// 		ss; // Pushed by the processor automatically.
// };

typedef struct __attribute__((packed)) pt_regs {
    /// FS and GS have no hardware-assigned uses.
    uint32_t gs;
    /// FS and GS have no hardware-assigned uses.
    uint32_t fs;
    /// Extra Segment determined by the programmer.
    uint32_t es;
    /// Data Segment.
    uint32_t ds;
    /// 32-bit destination register.
    uint32_t edi;
    /// 32-bit source register.
    uint32_t esi;
    /// 32-bit base pointer register.
    uint32_t ebp;
    /// 32-bit stack pointer register.
    uint32_t esp;
    /// 32-bit base register.
    uint32_t ebx;
    /// 32-bit data register.
    uint32_t edx;
    /// 32-bit counter.
    uint32_t ecx;
    /// 32-bit accumulator register.
    uint32_t eax;
    /// Interrupt number.
    uint32_t int_no;
    /// Error code.
    uint32_t err_code;
    /// Instruction Pointer Register.
    uint32_t eip;
    /// Code Segment.
    uint32_t cs;
    /// 32-bit flag register.
    uint32_t eflags;
    /// User application ESP.
    uint32_t useresp;
    /// Stack Segment.
    uint32_t ss;
} pt_regs;
