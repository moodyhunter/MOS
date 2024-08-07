// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <librpc/rpc.h>

#define DEVICE_MANAGER_RPCS_X(ARGS, PB, arg)                                                                                                                             \
    ARGS(arg, 1, register_device, REGISTER_DEVICE, "iiil", ARG(INT32, vendor), ARG(INT32, devid), ARG(INT32, location), ARG(INT64, mmio_base))                           \
    ARGS(arg, 2, register_driver, REGISTER_DRIVER, "v")

#define MOS_DEVICE_MANAGER_SERVICE_NAME "mos.device-manager"
