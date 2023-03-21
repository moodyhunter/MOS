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

#define STD_COLOR_LIGHT 0x8

typedef enum
{
    Black = 0x0,
    Blue = 0x1,
    Green = 0x2,
    Cyan = 0x3,
    Red = 0x4,
    Magenta = 0x5,
    Brown = 0x6,
    Gray = 0x7,
    DarkGray = Black | STD_COLOR_LIGHT,
    LightBlue = Blue | STD_COLOR_LIGHT,
    LightGreen = Green | STD_COLOR_LIGHT,
    LightCyan = Cyan | STD_COLOR_LIGHT,
    LightRed = Red | STD_COLOR_LIGHT,
    LightMagenta = Magenta | STD_COLOR_LIGHT,
    Yellow = Brown | STD_COLOR_LIGHT,
    White = Gray | STD_COLOR_LIGHT,
} standard_color_t;

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
