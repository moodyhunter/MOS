// SPDX-License-Identifier: GPL-3.0-or-later

#include <mos_stdio.h>
#include <mosapi.h>

int main(int argc, char *argv[])
{
    if (argc < 3)
    {
        puts("Usage: symlink <link> <target>");
        return 1;
    }

    if (syscall_vfs_symlink(argv[1], argv[2]))
    {
        printf("Failed to create symlink %s -> %s\n", argv[1], argv[2]);
        return 1;
    }

    printf("Created symlink %s -> %s\n", argv[1], argv[2]);
    return 0;
}
