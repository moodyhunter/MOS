// SPDX-License-Identifier: GPL-3.0-or-later

#include "librpc/rpc_client.h"
#include "x86_console/common.h"

#include <mos/device/dm_types.h>
#include <mos/syscall/usermode.h>
#include <stdio.h>
#include <stdlib.h>

static rpc_server_stub_t *console_server;

static void do_print_to_console(const char *buf, size_t size)
{
    rpc_call(console_server, CONSOLE_WRITE, NULL, "b", buf, size);
}

void open_console(void)
{
    if (!syscall_vfs_statat(FD_CWD, "/ipc/" X86_CONSOLE_SERVER_NAME, NULL))
    {
        printf("Spawning console driver...\n");
        pid_t pid = syscall_spawn("/initrd/drivers/x86_console_driver", 0, NULL);
        if (pid < 0)
            fatal_abort("Failed to spawn console driver.\n");
    }

    console_server = rpc_client_create("drivers.x86_text_console");
    rpc_call(console_server, DM_CONSOLE_CLEAR, NULL, "");
}

void print_to_console(const char *fmt, ...)
{
    char buf[1 KB];

    va_list args;
    va_start(args, fmt);
    int size = vsnprintf(buf, sizeof(buf), fmt, args);
    do_print_to_console(buf, size);
    va_end(args);
}

void set_console_color(standard_color_t fg, standard_color_t bg)
{
    rpc_call(console_server, DM_CONSOLE_SET_COLOR, NULL, "ii", fg, bg);
}
