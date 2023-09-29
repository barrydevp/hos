#pragma once

/// The name of the operating system.
#define OS_NAME "HOS"

/// The site of the operating system.
#define OS_SITEURL "https://github.com/barrydevp/hos"

/// The email of the reference developer.
#define OS_REF_EMAIL "barrydevp@gmail.com"

/// Major version of the operating system.
#define OS_MAJOR_VERSION 0

/// Minor version of the operating system.
#define OS_MINOR_VERSION 5

/// Micro version of the operating system.
#define OS_MICRO_VERSION 4

/// Helper to transform the given argument into a string.
#define OS_STR_HELPER(x) #x

/// Helper to transform the given argument into a string.
#define OS_STR(x) OS_STR_HELPER(x)

/// Complete version of the operating system.
#define OS_VERSION                                                             \
  OS_STR(OS_MAJOR_VERSION)                                                     \
  "." OS_STR(OS_MINOR_VERSION) "." OS_STR(OS_MICRO_VERSION)
