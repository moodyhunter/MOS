// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <librpc/rpc.h>
#include <mos/device/dm_types.h>

#define DM_RPCS_X(X, arg)                                                                                                                                                \
    X(arg, 1, register_device, REGISTER_DEVICE, "iiis", ARG(u32, vendor), ARG(u32, devid), ARG(u32, location), ARG(const char *, server))                                \
    X(arg, 2, register_driver, REGISTER_DRIVER, "v")

RPC_DEFINE_ENUMS(dm, DM, DM_RPCS_X)

#define MOS_DEVICE_MANAGER_SERVICE_NAME "mos.device_manager"

typedef struct device_t device_t;
typedef struct device_driver_t device_driver_t;

typedef struct device_driver_t
{
    const char *driver_name;
    const char *author;

    bool (*probe)(device_driver_t *driver, device_t *dev);
} device_driver_t;

typedef struct device_t
{
    const char *name;
    const char *description;
    device_type_t type;
    device_driver_t *driver;
} device_t;
