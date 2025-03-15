// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "proto/filesystem.pb.h"
#include "proto/filesystem.service.h"
#include "proto/userfs-manager.service.h"

#include <librpc/macro_magic.h>
#include <librpc/rpc.h>
#include <librpc/rpc_client.h>
#include <librpc/rpc_server++.hpp>
#include <mos/proto/fs_server.h>
#include <pb_decode.h>
#include <string>

class BlockdevFSServer : public IUserFSService
{
  public:
    explicit BlockdevFSServer(const std::string &servername) : IUserFSService(servername)
    {
    }

    virtual ~BlockdevFSServer() = default;

  private:
    rpc_result_code_t mount(rpc_context_t *, mosrpc_fs_mount_request *req, mosrpc_fs_mount_response *resp) override;
    rpc_result_code_t readdir(rpc_context_t *, mosrpc_fs_readdir_request *req, mosrpc_fs_readdir_response *resp) override;
    rpc_result_code_t lookup(rpc_context_t *, mosrpc_fs_lookup_request *req, mosrpc_fs_lookup_response *resp) override;
};
