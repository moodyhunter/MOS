// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "blockdev.h"
#include "gptdisk.hpp"

#include <librpc/rpc.h>
#include <librpc/rpc_client.h>
#include <librpc/rpc_server++.hpp>
#include <memory>
#include <pb_decode.h>

RPC_CLIENT_DEFINE_STUB_CLASS(BlockDevManagerServerStub, BLOCKDEV_MANAGER_RPC_X);
RPC_DECL_SERVER_INTERFACE_CLASS(IGPTLayerServer, BLOCKDEV_LAYER_RPC_X);

using namespace mos_rpc::blockdev;

extern std::unique_ptr<BlockDevManagerServerStub> manager;

class GPTLayerServer : public IGPTLayerServer
{
  public:
    explicit GPTLayerServer(std::shared_ptr<GPTDisk> disk, const std::string &servername);

  private:
    virtual rpc_result_code_t read_partition_block(rpc_context_t *context, mos_rpc_blockdev_read_partition_block_request *req, read_block::response *resp) override;
    virtual rpc_result_code_t write_partition_block(rpc_context_t *context, mos_rpc_blockdev_write_partition_block_request *req, write_block::response *resp) override;

  private:
    std::shared_ptr<GPTDisk> disk;
};
