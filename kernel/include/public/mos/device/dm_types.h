// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <mos/types.h>

/**
 * @defgroup device_manager.common Device Manager Common Types
 * @brief Device Manager Common Types
 * @{
 */

/**
 * @brief Device type
 */
typedef enum
{
    DEVICE_TYPE_CONSOLE,
    DEVICE_TYPE_BLOCK,
    DEVICE_TYPE_CHAR,
    DEVICE_TYPE_NET,
} device_type_t;

/** @} */
