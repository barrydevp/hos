#include <kernel/ctype.h>

/// Distance from a uppercase character to the correspondent lowercase in ASCII.
#define OFFSET 32

int isdigit(int c) {
  return (c >= 48 && c <= 57);
}

int isalpha(int c) {
  return ((c >= 65 && c <= 90) || (c >= 97 && c <= 122));
}

int iscntrl(int c) {
  return ((c >= 0x00 && c <= 0x1F) || (c == 0x7F));
}

int isalnum(int c) {
  return (isalpha(c) || isdigit(c));
}

int isxdigit(int c) {
  return (isdigit(c) || (c >= 65 && c <= 70));
}

int islower(int c) {
  return (c >= 97 && c <= 122);
}

int isupper(int c) {
  return (c >= 65 && c <= 90);
}

int tolower(int c) {
  if (isalpha(c) == 0 || islower(c)) {
    return c;
  }

  return c + OFFSET;
}

int toupper(int c) {
  if (isalpha(c) == 0 || isupper(c)) {
    return c;
  }

  return c - OFFSET;
}

int isspace(int c) {
  return (c == ' ');
}
