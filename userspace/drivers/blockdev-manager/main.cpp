// SPDX-License-Identifier: GPL-3.0-or-later

#include "blockdev_manager.hpp"
#include "libsm.h"

#include <iostream>

int main(int argc, char **argv)
{
    MOS_UNUSED(argc);
    MOS_UNUSED(argv);

    std::cerr << "Block Device Manager for MOS" << std::endl;
    ReportServiceState(UnitStatus::Starting, "blockdev-manager starting...");
    BlockManager manager;

    if (!register_blockdevfs())
    {
        std::cerr << "Failed to register blockdevfs" << std::endl;
        ReportServiceState(UnitStatus::Failed, "Failed to register blockdevfs");
        return 1;
    }

    ReportServiceState(UnitStatus::Started, "blockdev-manager started");
    manager.run();

    std::cerr << "Block Device Manager exiting" << std::endl;
    return 0;
}
