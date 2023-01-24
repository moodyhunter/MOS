// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/ipc/ipc_types.h"
#include "mos/types.h"

/**
 * @defgroup device_manager Device Manager
 * @brief Device Manager
 * @{
 */

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

/**
 * @brief Device type
 */
typedef enum
{
    DEVICE_TYPE_BLOCK,
    DEVICE_TYPE_CHAR,
    DEVICE_TYPE_NET,
} device_type_t;

typedef struct device_t
{
    const char *name;
    const char *description;
    device_type_t type;
    device_driver_t *driver;
} device_t;

void device_manager_init(void);

/** @} */
