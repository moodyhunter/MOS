// SPDX-License-Identifier: GPL-3.0-or-later

#include "x86_console/client.h"

#include <mos/device/dm_types.h>
#include <mos/syscall/usermode.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static rpc_server_stub_t *console_server;

rpc_server_stub_t *open_console(void)
{
    if (!syscall_vfs_statat(FD_CWD, "/ipc/" X86_CONSOLE_SERVER_NAME, NULL))
    {
        printf("Spawning console driver...\n");
        pid_t pid = syscall_spawn("/initrd/drivers/x86_console_driver", 0, NULL);
        if (pid < 0)
            fatal_abort("Failed to spawn console driver.\n");
    }

    console_server = rpc_client_create(X86_CONSOLE_SERVER_NAME);
    console_simple_clear(console_server);
    return console_server;
}

void print_to_console(const char *fmt, ...)
{
    char buf[1 KB];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    console_simple_write(console_server, buf);
    va_end(args);
}
