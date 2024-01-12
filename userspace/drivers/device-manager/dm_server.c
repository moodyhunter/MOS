// SPDX-License-Identifier: GPL-3.0-or-later

#include "dm.h"
#include "dm/common.h"

#include <librpc/rpc_server.h>
#include <stdio.h>

RPC_DECL_SERVER_PROTOTYPES(dm, DM_RPCS_X)

static int dm_register_device(rpc_server_t *server, rpc_args_iter_t *args, rpc_reply_t *reply, void *data)
{
    MOS_UNUSED(server);
    MOS_UNUSED(reply);
    MOS_UNUSED(data);

    rpc_result_code_t result = RPC_RESULT_OK;
    u32 vendor, device, location;
    u64 mmio_base;

    vendor = *(u32 *) rpc_arg_sized_next(args, sizeof(u32));
    device = *(u32 *) rpc_arg_sized_next(args, sizeof(u32));
    location = *(u32 *) rpc_arg_sized_next(args, sizeof(u32));
    mmio_base = *(u64 *) rpc_arg_sized_next(args, sizeof(u64));

    try_start_driver(vendor, device, location, mmio_base);
    return result;
}

static int dm_register_driver(rpc_server_t *server, rpc_args_iter_t *args, rpc_reply_t *reply, void *data)
{
    MOS_UNUSED(server);
    MOS_UNUSED(args);
    MOS_UNUSED(reply);
    MOS_UNUSED(data);

    rpc_result_code_t result = RPC_RESULT_OK;
    return result;
}

void dm_run_server(rpc_server_t *server)
{
    rpc_server_register_functions(server, dm_functions, MOS_ARRAY_SIZE(dm_functions));
    rpc_server_exec(server);
    fputs("device_manager: server exited\n", stderr);
}
