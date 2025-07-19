// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "blockdev.h"
#include "gptdisk.hpp"
#include "proto/blockdev.service.h"

#include <librpc/rpc.h>
#include <librpc/rpc_client.h>
#include <librpc/rpc_server++.hpp>
#include <memory>
#include <pb_decode.h>

using namespace mosrpc::blockdev;

extern std::unique_ptr<BlockdevManagerStub> manager;

class GPTLayerServer : public IBlockdevLayerService
{
  public:
    explicit GPTLayerServer(std::shared_ptr<GPTDisk> disk, const std::string &servername);

  private:
    virtual rpc_result_code_t read_partition_block(rpc_context_t *context, mosrpc_blockdev_read_partition_block_request *req, read_block::response *resp) override;
    virtual rpc_result_code_t write_partition_block(rpc_context_t *context, mosrpc_blockdev_write_partition_block_request *req, write_block::response *resp) override;

  private:
    std::shared_ptr<GPTDisk> disk;
};
