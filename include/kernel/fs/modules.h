#pragma once

#include <kernel/fs/vfs.h>

/// @brief Initialize the zero/null device.
/// @return 0 on success, 1 on failure.
int zero_init();

/// @brief Initialize the random device.
/// @return 0 on success, 1 on failure.
int random_init();
