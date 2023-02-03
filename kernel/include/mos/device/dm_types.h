// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/types.h"

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

typedef enum
{
    DM_CONSOLE_WRITE,
    DM_CONSOLE_READ,
    DM_CONSOLE_CLEAR,
    DM_CONSOLE_SET_COLOR,
    DM_CONSOLE_GET_COLOR,
    DM_CONSOLE_SET_CURSOR_POS,
    DM_CONSOLE_GET_CURSOR_POS,
    DM_CONSOLE_SET_CURSOR_VISIBLE,
    DM_CONSOLE_GET_CURSOR_VISIBLE,
} dm_console_cmd_t;

/** @} */
