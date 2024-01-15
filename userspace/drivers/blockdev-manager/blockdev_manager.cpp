// SPDX-License-Identifier: GPL-3.0-or-later

#include "blockdev_manager.hpp"

#include "autodestroy.hpp"
#include "blockdev.h"

#include <atomic>
#include <iostream>
#include <librpc/rpc_server.h>
#include <map>
#include <pb_decode.h>
#include <pb_encode.h>
#include <string>

RPC_DECL_SERVER_PROTOTYPES(blockdev_manager, BLOCKDEV_MANAGER_RPC_X)

rpc_server_t *blockdev_server = NULL;
std::map<int, blockdev_info> blockdev_list;    // blockdev id -> blockdev info
static std::atomic_ulong next_blockdev_id = 2; // 1 is reserved for the root directory

static int blockdev_manager_register_layer(rpc_server_t *server, mos_rpc_blockdev_register_layer_request *req, mos_rpc_blockdev_register_layer_response *resp, void *data)
{
    std::cout << "Got register_layer request" << std::endl;
    MOS_UNUSED(server);
    MOS_UNUSED(req);
    MOS_UNUSED(resp);
    MOS_UNUSED(data);
    return 0;
}

static int blockdev_manager_register_blockdev(rpc_server_t *server, mos_rpc_blockdev_register_dev_request *req, mos_rpc_blockdev_register_dev_response *resp, void *data)
{
    MOS_UNUSED(server);
    MOS_UNUSED(data);

    blockdev_info info = {
        .name = req->blockdev_name,
        .server_name = req->server_name,
        .num_blocks = req->num_blocks,
        .block_size = req->block_size,
        .ino = next_blockdev_id++,
    };

    blockdev_list[info.ino] = info;

    std::cout << "Registered blockdev " << info.name << " with id " << info.ino << std::endl;
    resp->result.success = true;
    resp->result.error = NULL;
    resp->id = info.ino;

    return 0;
}

bool blockdev_manager_run()
{
    auto blockdev_server = rpc_server_create(BLOCKDEV_MANAGER_RPC_SERVER_NAME, NULL);
    if (!blockdev_server)
    {
        std::cerr << "Failed to create RPC server" << std::endl;
        return false;
    }

    const auto guard = mAutoDestroy(blockdev_server, rpc_server_destroy);
    MOS_UNUSED(guard);

    rpc_server_register_functions(blockdev_server, blockdev_manager_functions, MOS_ARRAY_SIZE(blockdev_manager_functions));
    rpc_server_exec(blockdev_server);

    return true;
}
