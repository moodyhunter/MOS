// SPDX-License-Identifier: GPL-3.0-or-later

#include "lib/string.h"
#include "libuserspace.h"
#include "mos/syscall/usermode.h"

int main(void)
{
    printf("pong\n");
    fd_t client = syscall_ipc_connect("kmsg-ping-pong", IPC_CONNECT_DEFAULT, MOS_PAGE_SIZE);

    // client read
    char client_buf[150];
    size_t read_size = syscall_io_read(client, client_buf, 150, 0);
    printf("Client: Received '%.*s'\n", (int) read_size, client_buf);

    size_t written = syscall_io_write(client, "Nice Fox!", 10, 0);

    if (written != 10)
    {
        printf("Client: failed to write to ipc channel\n");
    }

    return 0;
}
