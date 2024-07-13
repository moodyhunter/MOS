// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "proto/filesystem.pb.h"

#include <librpc/macro_magic.h>
#include <librpc/rpc_client.h>
#include <librpc/rpc_server++.hpp>
#include <mos/proto/fs_server.h>
#include <pb_decode.h>
#include <string>

RPC_CLIENT_DEFINE_STUB_CLASS(UserfsManager, USERFS_MANAGER_X)
RPC_DECL_SERVER_INTERFACE_CLASS(IUserFSServer, USERFS_IMPL_X);

class BlockdevFSServer : public IUserFSServer
{
  public:
    explicit BlockdevFSServer(const std::string &servername) : IUserFSServer(servername)
    {
    }

    virtual ~BlockdevFSServer() = default;

  private:
    rpc_result_code_t mount(rpc_context_t *, mos_rpc_fs_mount_request *req, mos_rpc_fs_mount_response *resp) override;
    rpc_result_code_t readdir(rpc_context_t *, mos_rpc_fs_readdir_request *req, mos_rpc_fs_readdir_response *resp) override;
    rpc_result_code_t lookup(rpc_context_t *, mos_rpc_fs_lookup_request *req, mos_rpc_fs_lookup_response *resp) override;
    rpc_result_code_t readlink(rpc_context_t *, mos_rpc_fs_readlink_request *, mos_rpc_fs_readlink_response *resp) override;
    rpc_result_code_t getpage(rpc_context_t *, mos_rpc_fs_getpage_request *, mos_rpc_fs_getpage_response *resp) override;
};
