/// Copyright (c) 2014-2024 MentOs-Team
/// Copyright (c) 2022-2025 Minh Hai Dao (barrydevp)

#pragma once

#include <kernel/types.h>
#include <arch/i386/regs.h>

//! i86 defines 256 possible interrupt handlers (0-255)
#define I86_MAX_INTERRUPTS 256

//! must be in the format 0D110, where D is descriptor type
#define I86_IDT_DESC_BIT16   0x06 //00000110
#define I86_IDT_DESC_BIT32   0x0E //00001110
#define I86_IDT_DESC_RING1   0x40 //01000000
#define I86_IDT_DESC_RING2   0x20 //00100000
#define I86_IDT_DESC_RING3   0x60 //01100000
#define I86_IDT_DESC_PRESENT 0x80 //10000000

#define DISPATCHER_ISR 0x7F

struct __attribute__((packed)) idt_descriptor {
  uint16_t base_lo;
  uint16_t sel;
  uint8_t reserved;
  uint8_t flags;
  uint16_t base_hi;
};

struct __attribute__((packed)) idtr {
  uint16_t limit;
  uint32_t base;
};

#define IRQ_CONTINUE 0
#define IRQ_STOP     1

typedef void (*interrupt_handler_t)(pt_regs *regs);
typedef int32_t (*irq_handler_t)(pt_regs *regs);

void idt_init();
void irq_install_handler(uint32_t index, irq_handler_t handler);

void irq_ack(uint32_t irq_number);
void isr_handler(pt_regs *r);

/* ISRs reserved for CPU exceptions */
extern void isr0();
extern void isr1();
extern void isr2();
extern void isr3();
extern void isr4();
extern void isr5();
extern void isr6();
extern void isr7();
extern void isr8();
extern void isr9();
extern void isr10();
extern void isr11();
extern void isr12();
extern void isr13();
extern void isr14();
extern void isr15();
extern void isr16();
extern void isr17();
extern void isr18();
extern void isr19();
extern void isr20();
extern void isr21();
extern void isr22();
extern void isr23();
extern void isr24();
extern void isr25();
extern void isr26();
extern void isr27();
extern void isr28();
extern void isr29();
extern void isr30();
extern void isr31();
extern void isr127();
extern void isr128();

/* IRQ definitions */
extern void irq0();
extern void irq1();
extern void irq2();
extern void irq3();
extern void irq4();
extern void irq5();
extern void irq6();
extern void irq7();
extern void irq8();
extern void irq9();
extern void irq10();
extern void irq11();
extern void irq12();
extern void irq13();
extern void irq14();
extern void irq15();

#define IRQ0  32
#define IRQ1  33
#define IRQ2  34
#define IRQ3  35
#define IRQ4  36
#define IRQ5  37
#define IRQ6  38
#define IRQ7  39
#define IRQ8  40
#define IRQ9  41
#define IRQ10 42
#define IRQ11 43
#define IRQ12 44
#define IRQ13 45
#define IRQ14 46
#define IRQ15 47

/// @brief System timer.
#define IRQ_TIMER 0

/// @brief Keyboard controller.
#define IRQ_KEYBOARD 1

/// @brief cascaded signals from IRQs 8–15 (any devices configured to use IRQ
///        2 will actually be using IRQ 9)
#define IRQ_TO_SLAVE_PIC 2

/// @brief Serial port controller for serial port 2 (and 4).
#define IRQ_COM2_4 3

/// @brief Serial port controller for serial port 1 (and 3).
#define IRQ_COM1_3 4

/// @brief Parallel port 2 and 3 (or sound card).
#define IRQ_LPT2 5

/// @brief Floppy disk controller.
#define IRQ_FLOPPY 6

/// @brief Parallel port 1.
#define IRQ_LPT1 7

/// @brief Real-time clock (RTC).
#define IRQ_REAL_TIME_CLOCK 8

/// @brief Advanced Configuration and Power Interface (ACPI)
///        system control interrupt on Intel chipsets.[1] Other chipset
///        manufacturers might use another interrupt for this purpose, or
///        make it available for the use of peripherals (any devices configured
///        to use IRQ 2 will actually be using IRQ 9)
#define IRQ_AVAILABLE_1 9

/// @brief The Interrupt is left open for the use of
///        peripherals (open interrupt/available, SCSI or NIC).
#define IRQ_AVAILABLE_2 10

/// @brief The Interrupt is left open for the use of
///        peripherals (open interrupt/available, SCSI or NIC).
#define IRQ_AVAILABLE_3 11

/// @brief Mouse on PS/2 connector.
#define IRQ_MOUSE 12

/// @brief CPU co-processor or integrated floating point unit
///        or inter-processor interrupt (use depends on OS).
#define IRQ_MATH_CPU 13

/// @brief Primary ATA channel (ATA interface usually serves
///        hard disk drives and CD drives).
#define IRQ_FIRST_HD 14

/// @brief Secondary ATA channel.
#define IRQ_SECOND_HD 15
