// SPDX-License-Identifier: GPL-3.0-or-later

#include "text_mode_console.h"
#include "x86_console/common.h"

#include <librpc/rpc_server.h>
#include <stdlib.h>

DECLARE_RPC_SERVER_PROTOTYPES(x86_textmode_console, CONSOLE_RPCS_X)

static int x86_textmode_console_write(rpc_server_t *server, rpc_args_iter_t *args, rpc_reply_t *result, void *data)
{
    MOS_UNUSED(server);
    MOS_UNUSED(result);
    MOS_UNUSED(data);

    // arg: buffer
    size_t buffer_size;
    const char *buffer = rpc_arg_next(args, &buffer_size);

    screen_print_string(buffer, buffer_size);
    return RPC_RESULT_OK;
}

static int x86_textmode_console_clear(rpc_server_t *server, rpc_args_iter_t *args, rpc_reply_t *result, void *data)
{
    MOS_UNUSED(server);
    MOS_UNUSED(args);
    MOS_UNUSED(result);
    MOS_UNUSED(data);

    screen_clear();
    return RPC_RESULT_OK;
}

static int x86_textmode_console_set_color(rpc_server_t *server, rpc_args_iter_t *args, rpc_reply_t *result, void *data)
{
    MOS_UNUSED(server);
    MOS_UNUSED(result);
    MOS_UNUSED(data);

    const standard_color_t *foreground = rpc_arg_sized_next(args, sizeof(standard_color_t));
    if (foreground == NULL)
        return RPC_RESULT_INVALID_ARG;

    const standard_color_t *background = rpc_arg_sized_next(args, sizeof(standard_color_t));
    if (background == NULL)
        return RPC_RESULT_INVALID_ARG;

    screen_set_color(*foreground, *background);
    return RPC_RESULT_OK;
}

static int x86_textmode_console_set_cursor_pos(rpc_server_t *server, rpc_args_iter_t *args, rpc_reply_t *result, void *data)
{
    MOS_UNUSED(server);
    MOS_UNUSED(result);
    MOS_UNUSED(data);

    const u32 *x = rpc_arg_sized_next(args, sizeof(u32));
    if (x == NULL)
        return RPC_RESULT_INVALID_ARG;

    const u32 *y = rpc_arg_sized_next(args, sizeof(u32));
    if (y == NULL)
        return RPC_RESULT_INVALID_ARG;

    screen_set_cursor_pos(*x, *y);
    return RPC_RESULT_OK;
}

static int x86_textmode_console_set_cursor_visibility(rpc_server_t *server, rpc_args_iter_t *args, rpc_reply_t *result, void *data)
{
    MOS_UNUSED(server);
    MOS_UNUSED(result);
    MOS_UNUSED(data);

    const bool *visible = rpc_arg_sized_next(args, sizeof(bool));
    if (visible == NULL)
        return RPC_RESULT_INVALID_ARG;

    screen_enable_cursur(*visible);
    return RPC_RESULT_OK;
}

int main(int argc, char **argv)
{
    MOS_UNUSED(argc);
    MOS_UNUSED(argv);

    syscall_arch_syscall(X86_SYSCALL_IOPL_ENABLE, 0, 0, 0, 0);
    const ptr_t vaddr = syscall_arch_syscall(X86_SYSCALL_MAP_VGA_MEMORY, 0, 0, 0, 0);
    x86_vga_text_mode_console_init(vaddr);

    rpc_server_t *screen_server = rpc_server_create(X86_CONSOLE_SERVER_NAME, NULL);
    rpc_server_register_functions(screen_server, x86_textmode_console_functions, MOS_ARRAY_SIZE(x86_textmode_console_functions));
    rpc_server_exec(screen_server);
    return 0;
}
