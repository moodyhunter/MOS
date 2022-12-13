// SPDX-License-Identifier: GPL-3.0-or-later

#include "lib/string.h"
#include "libuserspace.h"
#include "mos/syscall/usermode.h"

int main(void)
{
    printf("pong\n");
    fd_t client = syscall_ipc_connect("kmsg-ping-pong", IPC_CONNECT_DEFAULT, MOS_PAGE_SIZE);

    // client read
    char *data = "ping";
    char client_buf[150];
    size_t client_read = syscall_io_read(client, client_buf, 150, 0);
    if (client_read != strlen(data))
    {
        printf("client: failed to read from ipc channel\n");
        return 1;
    }
    return 0;
}
