// SPDX-License-Identifier: GPL-3.0-or-later

#include <mos/syscall/usermode.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv)
{
    // mount <device> <mountpoint> <fstype>
    if (argc != 4)
    {
        puts("Usage: mount <device> <mountpoint> <fstype>");
        return -1;
    }

    const long ret = syscall_vfs_mount(argv[1], argv[2], argv[3], "");
    if (ret < 0)
    {
        printf("Failed to mount: %ld (%s)\n", ret, strerror(-ret));
        return -1;
    }

    return 0;
}
