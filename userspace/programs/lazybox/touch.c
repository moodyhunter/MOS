// SPDX-License-Identifier: GPL-3.0-or-later

#include <mos/filesystem/fs_types.h>
#include <mos/syscall/usermode.h>
#include <mos_stdio.h>

void do_touch(const char *path)
{
    if (syscall_vfs_openat(FD_CWD, path, OPEN_READ | OPEN_WRITE | OPEN_CREATE) < 0)
        fprintf(stderr, "failed to touch file '%s'\n", path);
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
