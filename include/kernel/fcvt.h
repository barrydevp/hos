#pragma once

/// @brief Thif function transforms `value` into a string of digits inside `buf`,
///         representing the whole part followed by the decimal part.
/// @details
/// For instance, 512.765 will result in:
///     decpt = 3
///     sign  = 0
///     buf   = "512765"
/// @param arg      The argument to turn into string.
/// @param chars    The total number of digits.
/// @param decpt    The position of the decimal point.
/// @param sign     The sign of the number.
/// @param buf      Buffer where the digits should be placed.
/// @param buf_size Dimension of the buffer.
void ecvtbuf(double arg, int chars, int *decpt, int *sign, char *buf,
             unsigned buf_size);

/// @brief Thif function transforms `value` into a string of digits inside `buf`,
///         representing the whole part followed by the decimal part.
/// @details
/// For instance, 512.765 will result in:
///     decpt = 3
///     sign  = 0
///     buf   = "512765"
/// @param arg      The argument to turn into string.
/// @param decimals The total number of digits to write after the decimal point.
/// @param decpt    The position of the decimal point.
/// @param sign     The sign of the number.
/// @param buf      Buffer where the digits should be placed.
/// @param buf_size Dimension of the buffer.
void fcvtbuf(double arg, int decimals, int *decpt, int *sign, char *buf,
             unsigned buf_size);
