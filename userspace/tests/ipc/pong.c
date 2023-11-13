// SPDX-License-Identifier: GPL-3.0-or-later

#include <mos/syscall/usermode.h>
#include <mos_stdio.h>
#include <mos_stdlib.h>
#include <mos_string.h>

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        printf("usage: %s <ping-pong-ipc-channel>\n", argv[0]);
        printf("connects to the given ipc name and reads a message from it\n");
        return 1;
    }

    const char *ipc_name = argv[1];
    printf("client: connecting to ipc name '%s'\n", ipc_name);

    const fd_t client = syscall_ipc_connect(ipc_name, MOS_PAGE_SIZE);
    if (client < 0)
    {
        printf("client: failed to open ipc channel '%s'\n", ipc_name);
        return 1;
    }

    size_t bufsize = 0;
    const size_t readsize = syscall_io_read(client, &bufsize, sizeof(bufsize));
    if (readsize != sizeof(bufsize))
    {
        puts("client: failed to read size from ipc channel");
        return 1;
    }

    char *buf = malloc(bufsize);
    const size_t read_size = syscall_io_read(client, buf, bufsize);
    if (read_size != bufsize)
    {
        puts("client: failed to read from ipc channel");
        return 1;
    }

    printf("client: received '%.*s'\n", (int) read_size, buf);

    const char *reply = "Hello, Server!";
    const size_t reply_size = strlen(reply) + 1;
    syscall_io_write(client, &reply_size, sizeof(reply_size));
    const size_t written = syscall_io_write(client, reply, reply_size);

    if (written != reply_size)
    {
        puts("client: failed to write to ipc channel");
    }

    return 0;
}
