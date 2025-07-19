// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "blockdev.h"
#include "proto/blockdev.service.h"

#include <abi-bits/ino_t.h>
#include <librpc/rpc_client.h>
#include <librpc/rpc_server++.hpp>
#include <librpc/rpc_server.h>
#include <map>
#include <pb_decode.h>
#include <pb_encode.h>
#include <string>
#include <variant>

using namespace mosrpc::blockdev;

struct BlockDeviceInfo
{
    std::string server_name;
};

struct BlockLayerInfo
{
    std::string server_name;
    u32 partid;
};

struct BlockInfo
{
    ino_t ino; // inode number in blockdevfs
    std::string name;
    size_t n_blocks;
    size_t block_size;

    enum
    {
        BLOCKDEV_LAYER,
        BLOCKDEV_DEVICE,
    } type;

    std::variant<BlockLayerInfo, BlockDeviceInfo> info;
};

extern std::map<std::string, BlockInfo> devices; // blockdev name -> blockdev info

class BlockManager : public IBlockdevManagerService
{

  public:
    explicit BlockManager() : IBlockdevManagerService(BLOCKDEV_MANAGER_RPC_SERVER_NAME) {};

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
