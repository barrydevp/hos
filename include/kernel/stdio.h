#pragma once

#include <kernel/types.h>

/// @brief The maximum number of digits of an integer.
#define MAX_DIGITS_IN_INTEGER 11
/// @brief The size of 'gets' buffer.
#define GETS_BUFFERSIZE 255

#ifndef EOF
/// @brief Define the End-Of-File.
#define EOF (-1)
#endif

// clang-format off
#define SEEK_SET 0 ///< The file offset is set to offset bytes.
#define SEEK_CUR 1 ///< The file offset is set to its current location plus offset bytes.
#define SEEK_END 2 ///< The file offset is set to the size of the file plus offset bytes.
// clang-format on

/// @brief Convert the given string to an integer.
/// @param str The string to convert.
/// @return The integer contained inside the string.
int atoi(const char *str);

/// @brief Converts the initial part of `str` to a long int value according to
///        the given base, which.
/// @param str    This is the string containing the integral number.
/// @param endptr Set to the character after the numerical value.
/// @param base   The base must be between 2 and 36 inclusive, or special 0.
/// @return Integral number as a long int value, else zero value is returned.
long strtol(const char *str, char **endptr, int base);

/// @brief Write formatted output to stdout.
/// @param fmt The format string.
/// @param ... The list of arguments.
/// @return On success, the total number of characters written is returned.
///         On failure, a negative number is returned.
int printf(const char *fmt, ...);

/// @brief Write formatted output to `str`.
/// @param str The buffer where the formatted string will be placed.
/// @param fmt  Format string, following the same specifications as printf.
/// @param ... The list of arguments.
/// @return On success, the total number of characters written is returned.
///         On failure, a negative number is returned.
int sprintf(char *str, const char *fmt, ...);

/// @brief Write formatted data from variable argument list to string.
/// @param str  Pointer to a buffer where the resulting C-string is stored.
/// @param fmt  Format string, following the same specifications as printf.
/// @param args A variable arguments list.
/// @return On success, the total number of characters written is returned.
///         On failure, a negative number is returned.
int vsprintf(char *str, const char *fmt, va_list args);
