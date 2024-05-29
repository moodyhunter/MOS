// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <abi-bits/ino_t.h>
#include <librpc/rpc_server.h>
#include <map>
#include <string>

struct blockdev_info
{
    std::string name;
    std::string server_name;
    size_t num_blocks;
    size_t block_size;

    ino_t ino; // inode number in blockdevfs
};

extern std::map<int, blockdev_info> devlist;

bool blockdev_manager_run();
bool register_blockdevfs();
