#pragma once

#include <arch/i386/pic.h>

/* Programmable interval timer */
#define PIT_A 0x40
#define PIT_B 0x41
#define PIT_C 0x42
#define PIT_CONTROL 0x43

#define PIT_MASK 0xFF
#define PIT_SCALE 1193180
#define PIT_SET 0x34

#define TIMER_IRQ 0

#define RESYNC_TIME 1

void pit_init(void);
