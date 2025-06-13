// SPDX-License-Identifier: GPL-3.0-or-later

#include "blockdev.h"
#include "ext4fs.hpp"
#include "libsm.h"
#include "mos/proto/fs_server.h"
#include "proto/blockdev.service.h"
#include "proto/userfs-manager.service.h"

#include <iostream>

#define DEBUG 1

std::unique_ptr<UserFSManagerStub> userfs_manager;
std::unique_ptr<BlockdevManagerStub> blockdev_manager;

int main(int argc, char **)
{
    std::cout << "EXT2/3/4 File System Driver for MOS" << std::endl;

    if (argc > 1)
        std::cerr << "Too many arguments" << std::endl, exit(1);

    blockdev_manager = std::make_unique<BlockdevManagerStub>(BLOCKDEV_MANAGER_RPC_SERVER_NAME);
    userfs_manager = std::make_unique<UserFSManagerStub>(USERFS_SERVER_RPC_NAME);

    const auto server_name = "fs.ext4"s;

#if DEBUG
    ext4_dmask_set(DEBUG_ALL);
#endif

    mosrpc_userfs_register_request req{
        .fs = { .name = strdup("ext4") },
        .rpc_server_name = strdup(server_name.c_str()),
    };
    mosrpc_userfs_register_response resp;
    const auto reg_result = userfs_manager->register_filesystem(&req, &resp);
    if (reg_result != RPC_RESULT_OK || !resp.result.success)
    {
        std::cerr << "Failed to register filesystem" << std::endl;
        if (resp.result.error)
            std::cerr << "Error: " << resp.result.error << std::endl;
        return 1;
    }

    pb_release(&mosrpc_userfs_register_request_msg, &req);
    pb_release(&mosrpc_userfs_register_response_msg, &resp);

    Ext4UserFS ext4_userfs(server_name);

    ReportServiceState(UnitStatus::Started, "ext4fs started");
    ext4_userfs.run();
    return 0;
}
