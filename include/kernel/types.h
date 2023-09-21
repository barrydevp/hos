#pragma once

#include <limits.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdarg.h>

#define asm __asm__
#define volatile __volatile__

#define ALIGN (sizeof(size_t))

#define ONES ((size_t)-1 / UCHAR_MAX)
#define HIGHS (ONES * (UCHAR_MAX / 2 + 1))
#define HASZERO(X) (((X)-ONES) & ~(X)&HIGHS)