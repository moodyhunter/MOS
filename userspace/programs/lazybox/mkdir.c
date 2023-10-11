// SPDX-License-Identifier: GPL-3.0-or-later

#include <mos/syscall/usermode.h>
#include <mos_stdio.h>

void do_mkdir(const char *path)
{
    if (syscall_vfs_mkdir(path) != 0)
        fprintf(stderr, "failed to create directory '%s'\n", path);
}

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        fputs("usage: mkdir <dir>...\n", stderr);
        return 1;
    }

    for (int i = 1; i < argc; i++)
        do_mkdir(argv[i]);

    return 0;
}
