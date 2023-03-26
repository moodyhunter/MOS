// SPDX-License-Identifier: GPL-3.0-or-later

#include <mos/syscall/usermode.h>
#include <stdio.h>

#define BUFSIZE 256

bool do_cat_file(const char *path)
{
    fd_t fd = syscall_vfs_open(path, OPEN_READ);
    if (fd < 0)
    {
        dprintf(stderr, "failed to open file '%s'\n", path);
        return false;
    }

    char buffer[BUFSIZE];

    do
    {
        size_t sz = syscall_io_read(fd, buffer, BUFSIZE);
        if (sz == 0)
            break;

        syscall_io_write(stdout, buffer, sz);
    } while (true);

    syscall_io_close(fd);

    return true;
}

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        dprintf(stderr, "usage: cat <file>...\n");
        return 1;
    }

    for (int i = 1; i < argc; i++)
        do_cat_file(argv[i]);

    return 0;
}
