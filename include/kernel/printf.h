#pragma once

#include <kernel/types.h>

__attribute__((format(__printf__,1,2)))
extern int printf(const char *fmt, ...);

extern size_t (*printf_output)(size_t, uint8_t *);

__attribute__((format(__printf__,2,3)))
extern int sprintf(char *str, const char *fmt, ...);

__attribute__((format(__printf__,3,4)))
extern int snprintf(char * str, size_t size, const char * format, ...);

extern size_t xvasprintf(int (*callback)(void *, char), void * userData, const char * fmt, va_list args);
int vsprintf(char *str, const char *fmt, va_list args);

__attribute__((format(__printf__,1,2)))
extern int dprintf(const char *fmt, ...);
extern void console_set_output(size_t (*output)(size_t,uint8_t*));

const char *to_human_size(unsigned long bytes);
const char *dec_to_binary(unsigned long value, unsigned length);
