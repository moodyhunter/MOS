// SPDX-License-Identifier: GPL-3.0-or-later

#include "lib/string.h"
#include "libuserspace.h"
#include "mos/syscall/usermode.h"

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        printf("Usage: pong <ping-pong-ipc-channel>\n");
        printf("Connects to the given ipc name and reads a message from it");
        return 1;
    }

    const char *ipc_name = argv[1];
    printf("Client: Connecting to ipc name '%s'\n", ipc_name);

    fd_t client = syscall_ipc_connect(ipc_name, MOS_PAGE_SIZE);
    if (client < 0)
    {
        printf("Client: failed to open ipc channel '%s'\n", ipc_name);
        return 1;
    }

    // client read
    char client_buf[150];
    size_t read_size = syscall_io_read(client, client_buf, 150);
    printf("Client: Received '%.*s'\n", (int) read_size, client_buf);

    size_t written = syscall_io_write(client, "Nice Fox!", 10);

    if (written != 10)
    {
        printf("Client: failed to write to ipc channel\n");
    }

    return 0;
}
