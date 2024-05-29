// SPDX-License-Identifier: GPL-3.0-or-later

#include "blockdev_manager.hpp"

#include "blockdev.h"

#include <atomic>
#include <iostream>
#include <librpc/rpc.h>
#include <librpc/rpc_server++.hpp>
#include <map>
#include <pb_decode.h>
#include <pb_encode.h>
#include <string>
#include <string_view>

RPC_DECL_SERVER_INTERFACE_CLASS(IBlockdevManager, bm, BLOCKDEV_MANAGER_RPC_X)

std::map<int, blockdev_info> devlist;          // blockdev id -> blockdev info
static std::atomic_ulong next_blockdev_id = 2; // 1 is reserved for the root directory

class BlockdevManager : public IBlockdevManager
{
  public:
    explicit BlockdevManager() : IBlockdevManager(BLOCKDEV_MANAGER_RPC_SERVER_NAME)
    {
    }

  public:
    virtual rpc_result_code_t bm_register_layer(rpc_context_t *, mos_rpc_blockdev_register_layer_request *req, mos_rpc_blockdev_register_layer_response *resp) override
    {
        std::cout << "Got register_layer request" << std::endl;
        MOS_UNUSED(req);
        MOS_UNUSED(resp);

        return RPC_RESULT_OK;
    }

    virtual rpc_result_code_t bm_register_blockdev(rpc_context_t *, mos_rpc_blockdev_register_dev_request *req, mos_rpc_blockdev_register_dev_response *resp) override
    {
        const auto it = std::ranges::find_if(devlist.begin(), devlist.end(), [&](const auto &dev) { return dev.second.name == req->blockdev_name; });
        if (it != devlist.end())
        {
            resp->result.success = false;
            resp->result.error = strdup("Blockdev already registered");
            return RPC_RESULT_OK;
        }

        const blockdev_info info = {
            .name = req->blockdev_name,
            .server_name = req->server_name,
            .num_blocks = req->num_blocks,
            .block_size = req->block_size,
            .ino = next_blockdev_id++,
        };

        devlist[info.ino] = info;

        std::cout << "Registered blockdev " << info.name << " with id " << info.ino << std::endl;
        resp->result.success = true;
        resp->result.error = NULL;
        resp->id = info.ino;

        return RPC_RESULT_OK;
    }

    virtual rpc_result_code_t bm_open_device(rpc_context_t *, mos_rpc_blockdev_opendev_request *req, mos_rpc_blockdev_opendev_response *resp) override
    {
        const auto name = std::string_view(req->device_name);
        for (const auto &blockdev : devlist)
        {
            if (blockdev.second.name == name)
            {
                resp->server_name = strdup(blockdev.second.server_name.c_str());
                resp->result.success = true;
                resp->result.error = NULL;
                return RPC_RESULT_OK;
            }
        }

        resp->result.success = false;
        resp->result.error = strdup("No such blockdev");

        return RPC_RESULT_OK;
    }
};

bool blockdev_manager_run()
{
    BlockdevManager manager;
    manager.run();
    return true;
}
