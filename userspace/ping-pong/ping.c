// SPDX-License-Identifier: GPL-2.0

#include "lib/string.h"
#include "libuserspace.h"
#include "mos/syscall/usermode.h"

int main(void)
{
    printf("ping\n");
    fd_t fd = syscall_ipc_open("kmsg-ping-pong", 0, MOS_PAGE_SIZE);
    if (fd < 0)
    {
        printf("failed to open ipc channel");
        return 1;
    }

    char *data = "ping";
    size_t written = syscall_io_write(fd, data, strlen(data), 0);
    if (written != strlen(data))
    {
        printf("failed to write to ipc channel");
        return 1;
    }
    char buf[150];
    size_t read = syscall_io_read(fd, buf, 150, 0);
    if (read != strlen(data))
    {
        printf("failed to read from ipc channel");
        return 1;
    }
    printf("pong: %s", buf);
    return 0;
}
