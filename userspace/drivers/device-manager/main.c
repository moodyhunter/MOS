// SPDX-License-Identifier: GPL-3.0-or-later

#include "dm/common.h"
#include "librpc/rpc_server.h"

#include <stdio.h>

DECLARE_RPC_SERVER_PROTOTYPES(dm, DM_RPC)

static int register_device(rpc_server_t *server, rpc_args_iter_t *args, rpc_reply_t *reply, void *data)
{
    MOS_UNUSED(server);
    MOS_UNUSED(args);
    MOS_UNUSED(reply);
    MOS_UNUSED(data);

    rpc_result_code_t result = RPC_RESULT_OK;
    return result;
}

static int register_driver(rpc_server_t *server, rpc_args_iter_t *args, rpc_reply_t *reply, void *data)
{
    MOS_UNUSED(server);
    MOS_UNUSED(args);
    MOS_UNUSED(reply);
    MOS_UNUSED(data);

    rpc_result_code_t result = RPC_RESULT_OK;
    return result;
}

int main(void)
{
    rpc_server_t *server = rpc_server_create(MOS_DEVICE_MANAGER_SERVICE_NAME, NULL);
    rpc_server_register_functions(server, dm_functions, MOS_ARRAY_SIZE(dm_functions));
    rpc_server_exec(server);
    fputs("device_manager: server exited\n", stderr);
    return 0;
}
