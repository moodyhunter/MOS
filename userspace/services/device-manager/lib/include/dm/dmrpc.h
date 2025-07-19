// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <librpc/rpc.h>

#define DEVICE_MANAGER_RPCS_X(ARGS, PB, arg)                                                                                                                             \
    ARGS(arg, 1, register_device, REGISTER_DEVICE, "iicccl", ARG(INT32, vendorid), ARG(INT32, deviceid), ARG(UINT8, busid), ARG(UINT8, devid), ARG(UINT8, funcid),       \
         ARG(INT64, mmio_base))                                                                                                                                          \
    ARGS(arg, 2, register_driver, REGISTER_DRIVER, "v")

#define MOS_DEVICE_MANAGER_SERVICE_NAME "mos.device-manager"
