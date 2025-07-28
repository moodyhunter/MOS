// SPDX-License-Identifier: GPL-3.0-or-later

#include "mosapi.h"

#include <mos/syscall/usermode.h>

#define MOUNTS_FILE "/sys/vfs/mount"

int main(int argc, char **argv)
{
    // mount <device> <mountpoint> <fstype>
    if (argc == 1)
    {
        // list mounts
        auto fd = open(MOUNTS_FILE, OPEN_READ);
        if (!fd)
        {
            printf("Failed to open " MOUNTS_FILE ", %s\n", strerror(-fd));
            return -1;
        }

        char line[256];
        while (fdgets(line, sizeof(line), fd))
            fputs(line, stdout);

        close(fd);
        return 0;
    }

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
