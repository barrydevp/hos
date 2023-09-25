#include <kernel/printf.h>
#include <arch/i386/serial.h>
#include <kernel/drivers/video.h>
#include <kernel/bitops.h>
#include <kernel/math.h>

int dprintf(const char *format, ...) {
  char buffer[4096];
  va_list ap;
  int len;
  // Start variabile argument's list.
  va_start(ap, format);
  len = vsprintf(buffer, format, ap);
  va_end(ap);

  const char *s = buffer;
  while ((*s) != 0)
    serial_output(SERIAL_PORT_A, *s++);

  // video_puts(buffer);

  return len;
}

const char *to_human_size(unsigned long bytes) {
  static char output[200];
  const char *suffix[] = { "B", "KB", "MB", "GB", "TB" };
  char length = sizeof(suffix) / sizeof(suffix[0]);
  int i = 0;
  double dblBytes = bytes;
  if (bytes > 1024) {
    for (i = 0; (bytes / 1024) > 0 && i < length - 1; i++, bytes /= 1024)
      dblBytes = bytes / 1024.0;
  }
  sprintf(output, "%.03lf %2s", dblBytes, suffix[i]);
  return output;
}

const char *dec_to_binary(unsigned long value, unsigned length) {
  static char buffer[33];
  for (int i = 0, j = 32 - min(max(0, length), 32); j < 32; ++i, ++j)
    buffer[i] = bit_check(value, 31 - j) ? '1' : '0';
  return buffer;
}
