// SPDX-License-Identifier: GPL-3.0-or-later

#include "lib/string.h"
#include "librpc/rpc.h"
#include "librpc/rpc_server.h"
#include "libuserspace.h"
#include "mos/device/console.h"
#include "mos/device/device_manager.h"
#include "mos/device/dm_types.h"
#include "mos/mos_global.h"
#include "mos/platform_syscall.h"
#include "mos/syscall/usermode.h"
#include "mos/x86/x86_platform.h"

int x86_textmode_console_write(rpc_server_t *server, rpc_args_iter_t *args, rpc_result_t *result, void *data);
int x86_textmode_console_clear(rpc_server_t *server, rpc_args_iter_t *args, rpc_result_t *result, void *data);
int x86_textmode_console_set_color(rpc_server_t *server, rpc_args_iter_t *args, rpc_result_t *result, void *data);

extern console_t vga_text_mode_console;

static const rpc_function_info_t console_functions[] = {
    { DM_CONSOLE_WRITE, x86_textmode_console_write, 2 },
    { DM_CONSOLE_CLEAR, x86_textmode_console_clear, 0 },
    { DM_CONSOLE_SET_COLOR, x86_textmode_console_set_color, 2 },
};

int main(int argc, char **argv)
{
    MOS_UNUSED(argc);
    MOS_UNUSED(argv);

    syscall_arch_syscall(X86_SYSCALL_IOPL_ENABLE, 0, 0, 0, 0);
    uintptr_t vaddr = syscall_arch_syscall(X86_SYSCALL_MAP_VGA_MEMORY, 0, 0, 0, 0);
    void x86_vga_text_mode_console_init(uintptr_t video_buffer_addr);
    x86_vga_text_mode_console_init(vaddr);

    rpc_server_t *screen_server = rpc_server_create("drivers.x86_text_console", NULL);
    rpc_server_register_functions(screen_server, console_functions, MOS_ARRAY_SIZE(console_functions));
    rpc_server_exec(screen_server);
    return 0;
}

int x86_textmode_console_write(rpc_server_t *server, rpc_args_iter_t *args, rpc_result_t *result, void *data)
{
    MOS_UNUSED(server);
    MOS_UNUSED(result);
    MOS_UNUSED(data);

    // args: buffer, buffer_size
    size_t buffer_size;
    const char *buffer = rpc_arg_next(args, &buffer_size);

    size_t size_size;
    const size_t *size = rpc_arg_next(args, &size_size);

    if (size_size != sizeof(size_t))
        return RPC_RESULT_SERVER_INVALID_ARG;

    if (buffer_size == 0 || size_size == 0)
        return RPC_RESULT_SERVER_INVALID_ARG;

    if (buffer_size != *size)
        return RPC_RESULT_SERVER_INVALID_ARG;

    vga_text_mode_console.write_impl(&vga_text_mode_console, buffer, buffer_size);
    return RPC_RESULT_OK;
}

int x86_textmode_console_clear(rpc_server_t *server, rpc_args_iter_t *args, rpc_result_t *result, void *data)
{
    MOS_UNUSED(server);
    MOS_UNUSED(args);
    MOS_UNUSED(result);
    MOS_UNUSED(data);
    vga_text_mode_console.clear(&vga_text_mode_console);
    return RPC_RESULT_OK;
}

int x86_textmode_console_set_color(rpc_server_t *server, rpc_args_iter_t *args, rpc_result_t *result, void *data)
{
    MOS_UNUSED(server);
    MOS_UNUSED(result);
    MOS_UNUSED(data);
    // args: foreground, background
    size_t foreground_size;
    const standard_color_t *foreground = rpc_arg_next(args, &foreground_size);

    if (foreground_size != sizeof(standard_color_t))
        return RPC_RESULT_SERVER_INVALID_ARG;

    size_t background_size;
    const standard_color_t *background = rpc_arg_next(args, &background_size);

    if (background_size != sizeof(standard_color_t))
        return RPC_RESULT_SERVER_INVALID_ARG;

    vga_text_mode_console.set_color(&vga_text_mode_console, *foreground, *background);
    return RPC_RESULT_OK;
}
