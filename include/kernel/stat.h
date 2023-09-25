#pragma once

#include <kernel/types.h>
#include <kernel/time.h>

/// @brief Data structure which contains information about a file.
typedef struct stat_t {
  /// ID of device containing file.
  dev_t st_dev;
  /// File serial number.
  ino_t st_ino;
  /// Mode of file.
  mode_t st_mode;
  /// File user id.
  uid_t st_uid;
  /// File group id.
  gid_t st_gid;
  /// File Size.
  off_t st_size;
  /// Time of last access.
  time_t st_atime;
  /// Time of last data modification.
  time_t st_mtime;
  /// Time of last status change.
  time_t st_ctime;
} stat_t;

/// @brief Retrieves information about the file at the given location.
/// @param path The path to the file that is being inquired.
/// @param buf  A structure where data about the file will be stored.
/// @return Returns a negative value on failure.
int stat(const char *path, stat_t *buf);

/// @brief Retrieves information about the file at the given location.
/// @param fd  The file descriptor of the file that is being inquired.
/// @param buf A structure where data about the file will be stored.
/// @return Returns a negative value on failure.
int fstat(int fd, stat_t *buf);

/// @brief Creates a new directory at the given path.
/// @param path The path of the new directory.
/// @param mode The permission of the new directory.
/// @return Returns a negative value on failure.
int mkdir(const char *path, mode_t mode);

/// @brief Removes the given directory.
/// @param path The path to the directory to remove.
/// @return Returns a negative value on failure.
int rmdir(const char *path);

/// @brief Creates a new file or rewrite an existing one.
/// @param path path to the file.
/// @param mode mode for file creation.
/// @return file descriptor number, -1 otherwise and errno is set to indicate the error.
/// @details
/// It is equivalent to: open(path, O_WRONLY|O_CREAT|O_TRUNC, mode)
int creat(const char *path, mode_t mode);
