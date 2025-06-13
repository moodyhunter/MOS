// SPDX-License-Identifier: GPL-3.0-or-later

#include "blockdev_manager.hpp"
#include "libsm.h"

#include <iostream>

int main(int argc, char **argv)
{
    MOS_UNUSED(argc);
    MOS_UNUSED(argv);

    std::cout << "Block Device Manager for MOS" << std::endl;
    BlockManager manager;

    if (!register_blockdevfs())
    {
        std::cerr << "Failed to register blockdevfs" << std::endl;
        ReportServiceState(UnitStatus::Failed, "Failed to register blockdevfs");
        return 1;
    }

    ReportServiceState(UnitStatus::Started, "manager started");
    manager.run();

    std::cout << "Block Device Manager exiting" << std::endl;
    return 0;
}
