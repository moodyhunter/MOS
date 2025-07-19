// SPDX-License-Identifier: GPL-3.0-or-later

#include "mosapi.h"

#include <mos/syscall/usermode.h>

int main(int argc, char **argv)
{
    // umount path
    if (argc != 2)
    {
        puts("Usage: umount <mountpoint>");
        return -1;
    }

    const long ret = syscall_vfs_unmount(argv[1]);
    if (ret < 0)
    {
        printf("Failed to unmount: %ld (%s)\n", ret, strerror(-ret));
        return -1;
    }

    return 0;
}
