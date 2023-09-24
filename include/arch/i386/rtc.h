#pragma once

#include <kernel/time.h>

/// @brief Copies the global time inside the provided variable.
/// @param time Pointer where we store the global time.
extern void gettime(tm_t *time);

/// @brief Initializes the Real Time Clock (RTC).
/// @return 0 on success, 1 on error.
int rtc_init();

/// @brief De-initializes the Real Time Clock (RTC).
/// @return 0 on success, 1 on error.
int rtc_finalize();
