// SPDX-License-Identifier: GPL-3.0-or-later

#include <mos/syscall/usermode.h>
#include <stdio.h>
#include <string.h>

#define MOUNTS_FILE "/sys/vfs/mount"

int main(int argc, char **argv)
{
    // mount <device> <mountpoint> <fstype>
    if (argc == 1)
    {
        // list mounts
        const auto f = fopen(MOUNTS_FILE, "r");
        if (!f)
        {
            puts("Failed to open " MOUNTS_FILE);
            return -1;
        }

        char line[256];
        while (fgets(line, sizeof(line), f))
            fputs(line, stdout);

        fclose(f);
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
