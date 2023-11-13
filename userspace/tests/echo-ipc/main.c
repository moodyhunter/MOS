// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/syscall/usermode.h"

#include <libipc/ipc.h>
#include <mos_stdio.h>
#include <mos_stdlib.h>

void ipc_do_echo(void *arg)
{
    fd_t client_fd = (fd_t) (ptr_t) arg;
    while (true)
    {
        ipc_msg_t *msg = ipc_read_msg(client_fd);

        if (msg == NULL)
            return; // EOF

        if (!ipc_write_msg(client_fd, msg))
        {
            puts("Failed to send IPC message");
            return;
        }

        ipc_msg_destroy(msg);
    }
}

int main()
{
    fd_t server_fd = syscall_ipc_create("echo-server", 30);
    if (IS_ERR_VALUE(server_fd))
    {
        puts("Failed to create IPC server");
        return 1;
    }

    while (true)
    {
        const fd_t client_fd = syscall_ipc_accept(server_fd);

        if (client_fd == 0)
            break; // closed

        if (IS_ERR_VALUE(client_fd))
        {
            puts("Failed to accept IPC client");
            return 1;
        }

        pthread_t *thread = malloc(sizeof(pthread_t));
        if (thread == NULL)
        {
            puts("Failed to allocate thread");
            return 1;
        }

        start_thread("echo-ipc", ipc_do_echo, (void *) (ptr_t) client_fd);
    }

    return 0;
}
