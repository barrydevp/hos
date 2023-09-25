#pragma once

/// @brief     Prints the given message and then safely stop the execution of
///            the kernel.
/// @param msg The message that has to be shown.
void kernel_panic(const char *msg);

#define TODO(msg) kernel_panic(#msg);
