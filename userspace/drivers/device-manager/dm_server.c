// SPDX-License-Identifier: GPL-3.0-or-later

#include "dm.h"
#include "dm/common.h"

#include <librpc/rpc_server.h>
#include <stdio.h>

RPC_DECL_SERVER_PROTOTYPES(dm, DM_RPCS_X)

static rpc_result_code_t dm_register_device(rpc_context_t *context, s32 vendor, s32 devid, s32 location, s64 mmio_base)
{
    MOS_UNUSED(context);
    rpc_result_code_t result = RPC_RESULT_OK;
    try_start_driver(vendor, devid, location, mmio_base);
    return result;
}

static rpc_result_code_t dm_register_driver(rpc_context_t *context)
{
    MOS_UNUSED(context);
    rpc_result_code_t result = RPC_RESULT_OK;
    return result;
}

void dm_run_server(rpc_server_t *server)
{
    rpc_server_register_functions(server, dm_functions, MOS_ARRAY_SIZE(dm_functions));
    rpc_server_exec(server);
    fputs("device_manager: server exited\n", stderr);
}
