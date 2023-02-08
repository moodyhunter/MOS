// SPDX-License-Identifier: GPL-3.0-or-later

#include "lib/memory.h"
#include "lib/stdio.h"
#include "lib/string.h"
#include "libuserspace.h"
#include "mos/syscall/usermode.h"

const char *data = "The quick brown fox jumps over the lazy dog.";
const size_t data_size = 43;

static void thread_main(void *arg)
{
    fd_t client_fd = (fd_t) arg;

    char buf[150];
    size_t sz = sprintf(buf, "Welcome to the server, client fd %d!", client_fd);

    size_t written = syscall_io_write(client_fd, buf, sz, 0);
    if (written != sz)
        printf("Server: failed to write to ipc channel\n");

    size_t read_size = syscall_io_read(client_fd, buf, 150, 0);
    printf("Server: Received '%.*s' from client %d\n", (int) read_size, buf, client_fd);

    syscall_io_close(client_fd);
}

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        printf("Usage: %s <ping-pong-ipc-channel>\n", argv[0]);
        printf("Starts a server that accepts connections on the given ipc name.\n");
        return -1;
    }

    const char *ipc_name = argv[1];
    printf("Server: Starting with ipc name '%s'\n", ipc_name);

    for (int i = 0; i < 10; i++)
        syscall_spawn("/programs/kmsg-pong", 1, &ipc_name);

    fd_t fd = syscall_ipc_create(ipc_name, 32);
    if (fd < 0)
    {
        printf("failed to open ipc channel\n");
        return -1;
    }

    while (true)
    {
        fd_t client_fd = syscall_ipc_accept(fd);
        if (client_fd < 0)
        {
            printf("failed to accept ipc channel\n");
            return -1;
        }

        printf("Server: Accepted fd %d\n", client_fd);
        start_thread("child", thread_main, (void *) client_fd);
    }

    return 0;
}
