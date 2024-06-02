// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "blockdev.h"

#include <abi-bits/ino_t.h>
#include <librpc/rpc_client.h>
#include <librpc/rpc_server++.hpp>
#include <librpc/rpc_server.h>
#include <map>
#include <pb_decode.h>
#include <pb_encode.h>
#include <string>

RPC_CLIENT_DEFINE_STUB_CLASS(BlockLayerServer, BLOCKDEV_LAYER_RPC_X);
RPC_CLIENT_DEFINE_STUB_CLASS(BlockDeviceServer, BLOCKDEV_DEVICE_RPC_X);
RPC_DECL_SERVER_INTERFACE_CLASS(IBlockManager, BLOCKDEV_MANAGER_RPC_X);

using namespace mos_rpc::blockdev;

struct BlockDeviceInfo
{
    std::string name, server_name;
    size_t num_blocks;
    size_t block_size;

    ino_t ino; // inode number in blockdevfs
};

extern std::map<std::string, BlockDeviceInfo> devices; // blockdev name -> blockdev info

class BlockManager : public IBlockManager
{

  public:
    explicit BlockManager() : IBlockManager(BLOCKDEV_MANAGER_RPC_SERVER_NAME){};

  private:
    virtual void on_connect(rpc_context_t *ctx) override;
    virtual void on_disconnect(rpc_context_t *ctx) override;
    virtual rpc_result_code_t register_layer_server(rpc_context_t *, register_layer_server::request *req, register_layer_server::response *resp) override;
    virtual rpc_result_code_t register_device(rpc_context_t *, register_device::request *req, register_device::response *resp) override;
    virtual rpc_result_code_t open_device(rpc_context_t *ctx, open_device::request *req, open_device::response *resp) override;
    virtual rpc_result_code_t read_block(rpc_context_t *, read_block::request *req, read_block::response *resp) override;
    virtual rpc_result_code_t write_block(rpc_context_t *, write_block::request *req, write_block::response *resp) override;
};

bool register_blockdevfs();
