// SPDX-License-Identifier: GPL-3.0-or-later

#include <librpc/rpc.h>
#include <mos/device/dm_types.h>

#pragma once

#define DM_RPC(_X)                                                                                                                                                       \
    _X(REGISTER_DEVICE, 1, register_device, 1)                                                                                                                           \
    _X(REGISTER_DRIVER, 2, register_driver, 1)

DECLARE_FUNCTION_ID_ENUM(dm, DM_RPC)

/**
 * @brief IPC service name for the device manager
 */
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
