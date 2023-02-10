// SPDX-License-Identifier: GPL-3.0-or-later

#include "lib/stdio.h"
#include "librpc/rpc_server.h"
#include "mos/mos_global.h"
#include "vfs_ops.h"

#define log(fmt, ...) printf(fmt "\n", ##__VA_ARGS__)

static rpc_server_t *vfs_server;

static rpc_function_info_t vfs_functions[VFSOP_MAX_OP] = {
    0,
};

int main(int argc, const char *argv[])
{
    log("Virtual Filesystem Daemon: " __DATE__ " " __TIME__);

    vfs_server = rpc_server_create("vfs.server", NULL);
    if (!vfs_server)
    {
        log("Failed to create vfs server");
        return 1;
    }

    rpc_server_register_functions(vfs_server, vfs_functions, MOS_ARRAY_SIZE(vfs_functions));
    rpc_server_exec(vfs_server);

    log("vfs daemon exiting");
    return 0;
}
