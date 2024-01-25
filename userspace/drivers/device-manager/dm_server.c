// SPDX-License-Identifier: GPL-3.0-or-later

#include "dm.h"
#include "dm/common.h"

#include <librpc/rpc_server.h>
#include <stdio.h>

RPC_DECL_SERVER_PROTOTYPES(dm, DM_RPCS_X)

static rpc_result_code_t dm_register_device(rpc_server_t *server, rpc_context_t *context, void *data)
{
    MOS_UNUSED(server);
    MOS_UNUSED(data);

    rpc_result_code_t result = RPC_RESULT_OK;

    s32 vendor = rpc_arg_s32(context, 0);
    s32 device = rpc_arg_s32(context, 1);
    s32 location = rpc_arg_s32(context, 2);
    ptr_t mmio_base = rpc_arg_s64(context, 3);

    try_start_driver(vendor, device, location, mmio_base);
    return result;
}

static rpc_result_code_t dm_register_driver(rpc_server_t *server, rpc_context_t *context, void *data)
{
    MOS_UNUSED(server);
    MOS_UNUSED(context);
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
