// SPDX-License-Identifier: GPL-3.0-or-later

#include <fcntl.h>
#include <mos_stdlib.h>

fd_t open(const char *path, open_flags flags)
{
    if (path == NULL)
        return -1;

    return openat(FD_CWD, path, flags);
}

fd_t openat(fd_t fd, const char *path, open_flags flags)
{
    return syscall_vfs_openat(fd, path, flags);
}
