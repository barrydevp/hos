#pragma once

#include <kernel/errno.h>

/// @brief Returns the string representing the error number.
/// @param errnum The error number.
/// @return The string representing the error number.
char *strerror(int errnum);
