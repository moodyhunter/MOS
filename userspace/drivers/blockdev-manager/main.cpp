// SPDX-License-Identifier: GPL-3.0-or-later

#include "blockdev_manager.hpp"

#include <iostream>

int main(int argc, char **argv)
{
    MOS_UNUSED(argc);
    MOS_UNUSED(argv);

    std::cerr << "Block Device Manager for MOS" << std::endl;
    BlockManager manager;

    if (!register_blockdevfs())
    {
        std::cerr << "Failed to register blockdevfs" << std::endl;
        return 1;
    }

    manager.run();

    std::cerr << "Block Device Manager exiting" << std::endl;
    return 0;
}
