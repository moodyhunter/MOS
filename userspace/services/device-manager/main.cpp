// SPDX-License-Identifier: GPL-3.0-or-later

#include "dm_server.hpp"
#include "libsm.h"

#include <librpc/rpc_server.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, const char *argv[])
{
    MOS_UNUSED(argc);
    MOS_UNUSED(argv);

    DeviceManagerServer dm_server;
    ReportServiceState(UnitStatus::Started, "DM started");

    dm_server.run();
    fputs("device_manager: server exited\n", stderr);

    return 0;
}
