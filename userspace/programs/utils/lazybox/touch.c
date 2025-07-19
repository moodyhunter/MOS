// SPDX-License-Identifier: GPL-3.0-or-later

#include "mosapi.h"

void do_touch(const char *path)
{
    fd_t fd = syscall_vfs_openat(AT_FDCWD, path, OPEN_READ | OPEN_WRITE | OPEN_CREATE);
    if (IS_ERR_VALUE(fd))
        fprintf(stderr, "failed to touch file '%s'\n", path);
    syscall_io_close(fd);
}

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        fputs("usage: touch <file>...\n", stderr);
        return 1;
    }

    for (int i = 1; i < argc; i++)
        do_touch(argv[i]);

    return 0;
}
