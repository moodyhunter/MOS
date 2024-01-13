// SPDX-License-Identifier: GPL-3.0-or-later

#include "blockdev_manager.hpp"

#include <iostream>

int main(int argc, char **argv)
{
    MOS_UNUSED(argc);
    MOS_UNUSED(argv);

    std::cerr << "Block Device Manager for MOS" << std::endl;
    if (!register_blockdevfs())
    {
        std::cerr << "Failed to register blockdevfs" << std::endl;
        return 1;
    }

    if (!blockdev_manager_run())
    {
        std::cerr << "Failed to start blockdev manager" << std::endl;
        return 1;
    }

    std::cerr << "Block Device Manager exiting" << std::endl;
    return 0;
}
