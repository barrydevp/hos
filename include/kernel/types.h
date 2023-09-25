#pragma once

#include <limits.h>
// #include <stdint.h>
// #include <stdbool.h>
// #include <stddef.h>
// #include <stdarg.h>

#include <kernel/limits.h>
#include <kernel/stdint.h>
#include <kernel/stdbool.h>
#include <kernel/stddef.h>
#include <kernel/stdarg.h>

#define asm __asm__
#define volatile __volatile__

#define ALIGN (sizeof(size_t))

#define ONES ((size_t)-1 / UCHAR_MAX)
#define HIGHS (ONES * (UCHAR_MAX / 2 + 1))
#define HASZERO(X) (((X)-ONES) & ~(X)&HIGHS)

/// The type of process id.
typedef signed int pid_t;

/// The type of process user variable.
typedef unsigned int user_t;

/// The type of process status.
typedef unsigned int status_t;

/// Type for system keys.
typedef int key_t;

/// Defines the list of flags of a process.
typedef enum eflags_list {
  /// Carry flag.
  EFLAG_CF = (1 << 0),

  /// Parity flag.
  EFLAG_PF = (1 << 2),

  /// Auxiliary carry flag.
  EFLAG_AF = (1 << 4),

  /// Zero flag.
  EFLAG_ZF = (1 << 6),

  /// Sign flag.
  EFLAG_SF = (1 << 7),

  /// Trap flag.
  EFLAG_TF = (1 << 8),

  /// Interrupt enable flag.
  EFLAG_IF = (1 << 9),

  /// Direction flag.
  EFLAG_DF = (1 << 10),

  /// Overflow flag.
  EFLAG_OF = (1 << 11),

  /// Nested task flag.
  EFLAG_NT = (1 << 14),

  ///  Resume flag.
  EFLAG_RF = (1 << 16),

  /// Virtual 8086 mode flag.
  EFLAG_VM = (1 << 17),

  /// Alignment check flag (486+).
  EFLAG_AC = (1 << 18),

  /// Virutal interrupt flag.
  EFLAG_VIF = (1 << 19),

  /// Virtual interrupt pending flag.
  EFLAG_VIP = (1 << 20),

  /// ID flag.
  EFLAG_ID = (1 << 21),
} eflags_list;
