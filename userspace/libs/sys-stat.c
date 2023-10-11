// SPDX-License-Identifier: GPL-3.0-or-later

#include <mos/filesystem/fs_types.h>
#include <mos_stdlib.h>
#include <sys/stat.h>

bool stat(const char *path, file_stat_t *buf)
{
    return syscall_vfs_fstatat(FD_CWD, path, buf, FSTATAT_NONE) == 0;
}

bool statat(int fd, const char *path, file_stat_t *buf)
{
    return syscall_vfs_fstatat(fd, path, buf, FSTATAT_NONE) == 0;
}

bool lstat(const char *path, file_stat_t *buf)
{
    return syscall_vfs_fstatat(FD_CWD, path, buf, FSTATAT_NOFOLLOW) == 0;
}

bool lstatat(int fd, const char *path, file_stat_t *buf)
{
    return syscall_vfs_fstatat(fd, path, buf, FSTATAT_NOFOLLOW) == 0;
}

bool fstat(fd_t fd, file_stat_t *buf)
{
    return syscall_vfs_fstatat(fd, NULL, buf, FSTATAT_FILE) == 0;
}

bool fstatat(fd_t fd, const char *path, file_stat_t *buf, fstatat_flags flags)
{
    return syscall_vfs_fstatat(fd, path, buf, flags) == 0;
}
