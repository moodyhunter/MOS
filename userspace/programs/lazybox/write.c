#include "mosapi.h"

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        printf("Usage: %s <file> contents...\n", argv[0]);
        return 1;
    }

    const char *path = argv[1];
    fd_t fd = open(path, OPEN_READ | OPEN_WRITE | OPEN_CREATE);
    if (IS_ERR_VALUE(fd))
    {
        printf("Failed to open %s\n", path);
        return 1;
    }

    for (int i = 2; i < argc; i++)
    {
        const char *buf = argv[i];
        size_t len = strlen(buf);
        if (syscall_io_write(fd, buf, len) != len)
        {
            printf("Failed to write to %s\n", path);
            return 1;
        }
    }

    syscall_io_close(fd);
    return 0;
}
