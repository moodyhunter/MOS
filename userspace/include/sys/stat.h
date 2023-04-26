// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <mos/filesystem/fs_types.h>

// stat a given path, may be relative to the current working directory
bool stat(const char *path, file_stat_t *buf);

// stat a given path, relative to a given directory
bool statat(int fd, const char *path, file_stat_t *buf);

// same as stat, but does not follow symlinks
bool lstat(const char *path, file_stat_t *buf);

// same as lstat, but relative to a given directory
bool lstatat(int fd, const char *path, file_stat_t *buf);

// stat an opened file descriptor
bool fstat(fd_t fd, file_stat_t *buf);

// stat a given path, may be relative to the current working directory
bool fstatat(fd_t fd, const char *path, file_stat_t *buf, fstatat_flags flags);
