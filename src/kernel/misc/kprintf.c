#include <kernel/printf.h>
#include <arch/i386/serial.h>
#include <kernel/drivers/video.h>

int dprintf(const char *format, ...) {
  char buffer[4096];
  va_list ap;
  int len;
  // Start variabile argument's list.
  va_start(ap, format);
  len = vsprintf(buffer, format, ap);
  va_end(ap);

  const char *s = buffer;
  while ((*s) != 0) { serial_output(SERIAL_PORT_A, *s++); }

  // video_puts(buffer);

  return len;
}
