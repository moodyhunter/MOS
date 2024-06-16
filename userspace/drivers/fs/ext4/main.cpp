// SPDX-License-Identifier: GPL-3.0-or-later

#include "ext4fs.hpp"

#include <iostream>

std::unique_ptr<UserFSManager> userfs_manager;
std::unique_ptr<BlockdevManager> blockdev_manager;

int main(int argc, char **)
{
    std::cout << "EXT2/3/4 File System Driver for MOS" << std::endl;

    if (argc > 1)
        std::cerr << "Too many arguments" << std::endl, exit(1);

    blockdev_manager = std::make_unique<BlockdevManager>(BLOCKDEV_MANAGER_RPC_SERVER_NAME);
    userfs_manager = std::make_unique<UserFSManager>(USERFS_SERVER_RPC_NAME);

    const auto server_name = "fs.ext4"s;

    mos_rpc_fs_register_request req{
        .fs = { .name = strdup("ext4") },
        .rpc_server_name = strdup(server_name.c_str()),
    };
    mos_rpc_fs_register_response resp;
    const auto reg_result = userfs_manager->register_fs(&req, &resp);
    if (reg_result != RPC_RESULT_OK || !resp.result.success)
    {
        std::cerr << "Failed to register filesystem" << std::endl;
        if (resp.result.error)
            std::cerr << "Error: " << resp.result.error << std::endl;
        return 1;
    }

    pb_release(&mos_rpc_fs_register_request_msg, &req);
    pb_release(&mos_rpc_fs_register_response_msg, &resp);

    Ext4UserFS ext4_userfs(server_name);
    ext4_userfs.run();
    return 0;
}
