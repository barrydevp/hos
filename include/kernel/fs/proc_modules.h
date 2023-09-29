#pragma once

#include <kernel/fs/vfs.h>

/// @brief Initialize the procfs video files.
/// @return 0 on success, 1 on failure.
int procv_module_init();

/// @brief Initialize the procfs system files.
/// @return 0 on success, 1 on failure.
int procs_module_init();
