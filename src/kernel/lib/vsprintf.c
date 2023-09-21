#include <kernel/types.h>
#include <kernel/math.h>
#include <kernel/ctype.h>
#include <kernel/string.h>
#include <kernel/fcvt.h>
#include <kernel/drivers/vga.h>

/// Size of the buffer used to call cvt functions.
#define CVTBUFSIZE 500

#define FLAGS_ZEROPAD (1U << 0U) ///< Fill zeros before the number.
#define FLAGS_LEFT (1U << 1U) ///< Left align the value.
#define FLAGS_PLUS (1U << 2U) ///< Print the plus sign.
#define FLAGS_SPACE \
  (1U << 3U) ///< If positive add a space instead of the plus sign.
#define FLAGS_HASH (1U << 4U) ///< Preceed with 0x or 0X, %x or %X respectively.
#define FLAGS_UPPERCASE (1U << 5U) ///< Print uppercase.
#define FLAGS_SIGN (1U << 6U) ///< Print the sign.

/// The list of digits.
static char *_digits = "0123456789abcdefghijklmnopqrstuvwxyz";

/// The list of uppercase digits.
static char *_upper_digits = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";

/// @brief Returns the index of the first non-integer character.
static inline int skip_atoi(const char **s) {
  int i = 0;
  while (isdigit(**s))
    i = i * 10 + *((*s)++) - '0';
  return i;
}

static char *number(char *str, long num, int base, int size, int32_t precision,
                    unsigned flags) {
  char c, tmp[66] = { 0 };
  char *dig = _digits;

  if (flags & FLAGS_UPPERCASE) {
    dig = _upper_digits;
  }
  if (flags & FLAGS_LEFT) {
    flags &= ~FLAGS_ZEROPAD;
  }
  if (base < 2 || base > 36) {
    return 0;
  }

  c = (flags & FLAGS_ZEROPAD) ? '0' : ' ';

  // --------------------------------
  // Set the sign.
  // --------------------------------
  char sign = 0;
  if (flags & FLAGS_SIGN) {
    if (num < 0) {
      sign = '-';
      num = -num;
      size--;
    } else if (flags & FLAGS_PLUS) {
      sign = '+';
      size--;
    } else if (flags & FLAGS_SPACE) {
      sign = ' ';
      size--;
    }
  }
  // Sice I've removed the sign (if negative), i can transform it to unsigned.
  uint32_t uns_num = (uint32_t)num;
  if (flags & FLAGS_HASH) {
    if (base == 16) {
      size -= 2;
    } else if (base == 8) {
      size--;
    }
  }

  int32_t i = 0;
  if (uns_num == 0) {
    tmp[i++] = '0';
  } else {
    while (uns_num != 0) {
      tmp[i++] = dig[((unsigned long)uns_num) % (unsigned)base];
      uns_num = ((unsigned long)uns_num) / (unsigned)base;
    }
  }
  if (i > precision) {
    precision = i;
  }
  size -= precision;
  if (!(flags & (FLAGS_ZEROPAD | FLAGS_LEFT))) {
    while (size-- > 0)
      *str++ = ' ';
  }
  if (sign) {
    *str++ = sign;
  }
  if (flags & FLAGS_HASH) {
    if (base == 8)
      *str++ = '0';
    else if (base == 16) {
      *str++ = '0';
      *str++ = _digits[33];
    }
  }
  if (!(flags & FLAGS_LEFT)) {
    while (size-- > 0) {
      *str++ = c;
    }
  }
  while (i < precision--) {
    *str++ = '0';
  }
  while (i-- > 0) {
    *str++ = tmp[i];
  }
  while (size-- > 0) {
    *str++ = ' ';
  }
  return str;
}

static char *eaddr(char *str, unsigned char *addr, int size, int precision,
                   unsigned flags) {
  (void)precision;
  char tmp[24];
  char *dig = _digits;
  int i, len;

  if (flags & FLAGS_UPPERCASE) {
    dig = _upper_digits;
  }

  len = 0;
  for (i = 0; i < 6; i++) {
    if (i != 0) {
      tmp[len++] = ':';
    }
    tmp[len++] = dig[addr[i] >> 4];
    tmp[len++] = dig[addr[i] & 0x0F];
  }

  if (!(flags & FLAGS_LEFT)) {
    while (len < size--) {
      *str++ = ' ';
    }
  }

  for (i = 0; i < len; ++i) {
    *str++ = tmp[i];
  }

  while (len < size--) {
    *str++ = ' ';
  }

  return str;
}

static char *iaddr(char *str, unsigned char *addr, int size, int precision,
                   unsigned flags) {
  (void)precision;
  char tmp[24];
  int i, n, len;

  len = 0;
  for (i = 0; i < 4; i++) {
    if (i != 0) {
      tmp[len++] = '.';
    }
    n = addr[i];

    if (n == 0) {
      tmp[len++] = _digits[0];
    } else {
      if (n >= 100) {
        tmp[len++] = _digits[n / 100];
        n = n % 100;
        tmp[len++] = _digits[n / 10];
        n = n % 10;
      } else if (n >= 10) {
        tmp[len++] = _digits[n / 10];
        n = n % 10;
      }

      tmp[len++] = _digits[n];
    }
  }

  if (!(flags & FLAGS_LEFT)) {
    while (len < size--) {
      *str++ = ' ';
    }
  }

  for (i = 0; i < len; ++i) {
    *str++ = tmp[i];
  }

  while (len < size--) {
    *str++ = ' ';
  }

  return str;
}

static void cfltcvt(double value, char *buffer, char fmt, int precision) {
  int decpt, sign, exp, pos;
  char cvtbuf[CVTBUFSIZE];
  char *digits = cvtbuf;
  int capexp = 0;
  int magnitude;

  if (fmt == 'G' || fmt == 'E') {
    capexp = 1;
    fmt += 'a' - 'A';
  }

  if (fmt == 'g') {
    ecvtbuf(value, precision, &decpt, &sign, cvtbuf, CVTBUFSIZE);
    magnitude = decpt - 1;
    if (magnitude < -4 || magnitude > precision - 1) {
      fmt = 'e';
      precision -= 1;
    } else {
      fmt = 'f';
      precision -= decpt;
    }
  }

  if (fmt == 'e') {
    ecvtbuf(value, precision + 1, &decpt, &sign, cvtbuf, CVTBUFSIZE);

    if (sign) {
      *buffer++ = '-';
    }
    *buffer++ = *digits;
    if (precision > 0) {
      *buffer++ = '.';
    }
    memcpy(buffer, digits + 1, precision);
    buffer += precision;
    *buffer++ = capexp ? 'E' : 'e';

    if (decpt == 0) {
      if (value == 0.0) {
        exp = 0;
      } else {
        exp = -1;
      }
    } else {
      exp = decpt - 1;
    }

    if (exp < 0) {
      *buffer++ = '-';
      exp = -exp;
    } else {
      *buffer++ = '+';
    }

    buffer[2] = (char)((exp % 10) + '0');
    exp = exp / 10;
    buffer[1] = (char)((exp % 10) + '0');
    exp = exp / 10;
    buffer[0] = (char)((exp % 10) + '0');
    buffer += 3;
  } else if (fmt == 'f') {
    fcvtbuf(value, precision, &decpt, &sign, cvtbuf, CVTBUFSIZE);
    if (sign) {
      *buffer++ = '-';
    }
    if (*digits) {
      if (decpt <= 0) {
        *buffer++ = '0';
        *buffer++ = '.';
        for (pos = 0; pos < -decpt; pos++) {
          *buffer++ = '0';
        }
        while (*digits) {
          *buffer++ = *digits++;
        }
      } else {
        pos = 0;
        while (*digits) {
          if (pos++ == decpt) {
            *buffer++ = '.';
          }
          *buffer++ = *digits++;
        }
      }
    } else {
      *buffer++ = '0';
      if (precision > 0) {
        *buffer++ = '.';
        for (pos = 0; pos < precision; pos++) {
          *buffer++ = '0';
        }
      }
    }
  }

  *buffer = '\0';
}

static void forcdecpt(char *buffer) {
  while (*buffer) {
    if (*buffer == '.') {
      return;
    }
    if (*buffer == 'e' || *buffer == 'E') {
      break;
    }

    buffer++;
  }

  if (*buffer) {
    long n = (long)strlen(buffer);
    while (n > 0) {
      buffer[n + 1] = buffer[n];
      n--;
    }
    *buffer = '.';
  } else {
    *buffer++ = '.';
    *buffer = '\0';
  }
}

static void cropzeros(char *buffer) {
  char *stop;

  while (*buffer && *buffer != '.') {
    buffer++;
  }

  if (*buffer++) {
    while (*buffer && *buffer != 'e' && *buffer != 'E') {
      buffer++;
    }
    stop = buffer--;
    while (*buffer == '0') {
      buffer--;
    }
    if (*buffer == '.') {
      buffer--;
    }
    while ((*++buffer = *stop++)) {
    }
  }
}

static char *flt(char *str, double num, int size, int precision, char fmt,
                 unsigned flags) {
  char tmp[80];
  char c, sign;
  int n, i;

  // Left align means no zero padding.
  if (flags & FLAGS_LEFT)
    flags &= ~FLAGS_ZEROPAD;

  // Determine padding and sign char.
  c = (flags & FLAGS_ZEROPAD) ? '0' : ' ';
  sign = 0;
  if (flags & FLAGS_SIGN) {
    if (num < 0.0) {
      sign = '-';
      num = -num;
      size--;
    } else if (flags & FLAGS_PLUS) {
      sign = '+';
      size--;
    } else if (flags & FLAGS_SPACE) {
      sign = ' ';
      size--;
    }
  }

  // Compute the precision value.
  if (precision < 0) {
    // Default precision: 6.
    precision = 6;
  } else if (precision == 0 && fmt == 'g') {
    // ANSI specified.
    precision = 1;
  }

  // Convert floating point number to text.
  cfltcvt(num, tmp, fmt, precision);

  // '#' and precision == 0 means force a decimal point.
  if ((flags & FLAGS_HASH) && precision == 0) {
    forcdecpt(tmp);
  }

  // 'g' format means crop zero unless '#' given.
  if (fmt == 'g' && !(flags & FLAGS_HASH)) {
    cropzeros(tmp);
  }

  n = strlen(tmp);

  // Output number with alignment and padding.
  size -= n;
  if (!(flags & (FLAGS_ZEROPAD | FLAGS_LEFT))) {
    while (size-- > 0) {
      *str++ = ' ';
    }
  }

  if (sign) {
    *str++ = sign;
  }

  if (!(flags & FLAGS_LEFT)) {
    while (size-- > 0) {
      *str++ = c;
    }
  }

  for (i = 0; i < n; i++) {
    *str++ = tmp[i];
  }

  while (size-- > 0) {
    *str++ = ' ';
  }

  return str;
}

int vsprintf(char *str, const char *fmt, va_list args) {
  int base;
  char *tmp;
  char *s;

  // Flags to number().
  unsigned flags;

  // 'h', 'l', or 'L' for integer fields.
  char qualifier;

  for (tmp = str; *fmt; fmt++) {
    if (*fmt != '%') {
      *tmp++ = *fmt;

      continue;
    }

    // Process flags-
    flags = 0;
repeat:
    // This also skips first '%'.
    fmt++;
    switch (*fmt) {
      case '-':
        flags |= FLAGS_LEFT;
        goto repeat;
      case '+':
        flags |= FLAGS_PLUS;
        goto repeat;
      case ' ':
        flags |= FLAGS_SPACE;
        goto repeat;
      case '#':
        flags |= FLAGS_HASH;
        goto repeat;
      case '0':
        flags |= FLAGS_ZEROPAD;
        goto repeat;
    }

    // Get the width of the output field.
    int32_t field_width;
    field_width = -1;

    if (isdigit(*fmt)) {
      field_width = skip_atoi(&fmt);
    } else if (*fmt == '*') {
      fmt++;
      field_width = va_arg(args, int32_t);
      if (field_width < 0) {
        field_width = -field_width;
        flags |= FLAGS_LEFT;
      }
    }

    /* Get the precision, thus the minimum number of digits for
         * integers; max number of chars for from string.
         */
    int32_t precision = -1;
    if (*fmt == '.') {
      ++fmt;
      if (isdigit(*fmt)) {
        precision = skip_atoi(&fmt);
      } else if (*fmt == '*') {
        ++fmt;
        precision = va_arg(args, int);
      }
      if (precision < 0) {
        precision = 0;
      }
    }

    // Get the conversion qualifier.
    qualifier = -1;
    if (*fmt == 'h' || *fmt == 'l' || *fmt == 'L') {
      qualifier = *fmt;
      fmt++;
    }

    // Default base.
    base = 10;

    switch (*fmt) {
      case 'c':
        if (!(flags & FLAGS_LEFT)) {
          while (--field_width > 0) {
            *tmp++ = ' ';
          }
        }
        *tmp++ = va_arg(args, char);
        while (--field_width > 0) {
          *tmp++ = ' ';
        }
        continue;

      case 's':
        s = va_arg(args, char *);
        if (!s) {
          s = "<NULL>";
        }

        int32_t len = (int32_t)strnlen(s, (uint32_t)precision);
        if (!(flags & FLAGS_LEFT)) {
          while (len < field_width--) {
            *tmp++ = ' ';
          }
        }

        int32_t it;
        for (it = 0; it < len; ++it) {
          *tmp++ = *s++;
        }
        while (len < field_width--) {
          *tmp++ = ' ';
        }
        continue;

      case 'p':
        if (field_width == -1) {
          field_width = 2 * sizeof(void *);
          flags |= FLAGS_ZEROPAD;
        }
        tmp = number(tmp, (unsigned long)va_arg(args, void *), 16, field_width,
                     precision, flags);
        continue;
      case 'n':
        if (qualifier == 'l') {
          long *ip = va_arg(args, long *);
          *ip = (tmp - str);
        } else {
          int *ip = va_arg(args, int *);
          *ip = (tmp - str);
        }
        continue;
      case 'A':
        flags |= FLAGS_UPPERCASE;
        break;
      case 'a':
        if (qualifier == 'l')
          tmp = eaddr(tmp, va_arg(args, unsigned char *), field_width,
                      precision, flags);
        else
          tmp = iaddr(tmp, va_arg(args, unsigned char *), field_width,
                      precision, flags);
        continue;
        // Integer number formats - set up the flags and "break".
      case 'o':
        base = 8;
        break;

      case 'X':
        flags |= FLAGS_UPPERCASE;
        break;

      case 'x':
        base = 16;
        break;

      case 'd':
      case 'i':
        flags |= FLAGS_SIGN;

      case 'u':
        break;
      case 'E':
      case 'G':
      case 'e':
      case 'f':
      case 'g':
        tmp = flt(tmp, va_arg(args, double), field_width, precision, *fmt,
                  flags | FLAGS_SIGN);
        continue;
      default:
        if (*fmt != '%')
          *tmp++ = '%';
        if (*fmt)
          *tmp++ = *fmt;
        else
          --fmt;
        continue;
    }

    if (flags & FLAGS_SIGN) {
      long num;
      if (qualifier == 'l') {
        num = va_arg(args, long);
      } else if (qualifier == 'h') {
        num = va_arg(args, short);
      } else {
        num = va_arg(args, int);
      }
      tmp = number(tmp, num, base, field_width, precision, flags);
    } else {
      unsigned long num;
      if (qualifier == 'l') {
        num = va_arg(args, unsigned long);
      } else if (qualifier == 'h') {
        num = va_arg(args, unsigned short);
      } else {
        num = va_arg(args, unsigned int);
      }
      tmp = number(tmp, num, base, field_width, precision, flags);
    }
  }

  *tmp = '\0';
  return tmp - str;
}

int printf(const char *format, ...) {
  char buffer[4096];
  va_list ap;
  int len;
  // Start variabile argument's list.
  va_start(ap, format);
  len = vsprintf(buffer, format, ap);
  va_end(ap);
  // video_puts(buffer);
  vga_puts(buffer);
  return len;
}

int sprintf(char *str, const char *fmt, ...) {
  va_list args;
  int len;

  va_start(args, fmt);
  len = vsprintf(str, fmt, args);
  va_end(args);

  return len;
}
