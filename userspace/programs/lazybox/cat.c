// SPDX-License-Identifier: GPL-3.0-or-later

#include <fcntl.h>
#include <mos/syscall/usermode.h>
#include <mos_stdio.h>

#define BUFSIZE 4096

bool do_cat_file(const char *path)
{
    fd_t fd = open(path, OPEN_READ);
    if (fd < 0)
    {
        fprintf(stderr, "failed to open file '%s'\n", path);
        return false;
    }

    do
    {
        char buffer[BUFSIZE] = { 0 };
        size_t sz = syscall_io_read(fd, buffer, BUFSIZE);
        if (sz == 0)
            break;

        if (sz == (size_t) -1)
        {
            fprintf(stderr, "failed to read file '%s'\n", path);
            syscall_io_close(fd);
            return false;
        }

        fwrite(buffer, 1, sz, stdout);
    } while (true);

    syscall_io_close(fd);

    return true;
}

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        fputs("usage: cat <file>...\n", stderr);
        return 1;
    }

    for (int i = 1; i < argc; i++)
        do_cat_file(argv[i]);

    return 0;
}
