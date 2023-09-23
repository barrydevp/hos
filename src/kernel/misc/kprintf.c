#include <kernel/printf.h>
#include <kernel/drivers/video.h>

// int printf(const char *fmt, ...) {
//   va_list args;
//   va_start(args, fmt);
//   vga_puts((char *)fmt);
//   // int out = xvasprintf(cb_printf, NULL, fmt, args);
//   va_end(args);
//   // return out;
//   return 0;
// }

// int dprintf(const char *fmt, ...) {
//   va_list args;
//   va_start(args, fmt);
//   vga_puts((char *)fmt);
//   // int out = xvasprintf(cb_printf, NULL, fmt, args);
//   va_end(args);
//   // return out;
//   return 0;
// }
