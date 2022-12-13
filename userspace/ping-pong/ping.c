// SPDX-License-Identifier: GPL-3.0-or-later

#include "lib/string.h"
#include "libuserspace.h"
#include "mos/syscall/usermode.h"

int main(void)
{
    syscall_spawn("/programs/kmsg-pong", 0, NULL);
    printf("ping\n");
    fd_t fd = syscall_ipc_create("kmsg-ping-pong", 32);
    if (fd < 0)
    {
        printf("failed to open ipc channel\n");
        return -1;
    }

    fd_t client_fd;
    while ((client_fd = syscall_ipc_accept(fd)) < 0)
        ;

    char *data = "ping";
    size_t written = syscall_io_write(client_fd, data, strlen(data), 0);
    if (written != strlen(data))
    {
        printf("server: failed to write to ipc channel\n");
    }

    while (1)
        ;

    return 0;
}
