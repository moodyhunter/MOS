// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <librpc/rpc.h>
#include <mos/device/dm_types.h>

#define CONSOLE_RPCS(_X)                                                                                                                                                 \
    _X(CONSOLE_WRITE, DM_CONSOLE_WRITE, x86_textmode_console_write, 1)                                                                                                   \
    _X(CONSOLE_CLEAR, DM_CONSOLE_CLEAR, x86_textmode_console_clear, 0)                                                                                                   \
    _X(CONSOLE_SET_COLOR, DM_CONSOLE_SET_COLOR, x86_textmode_console_set_color, 2)                                                                                       \
    _X(CONSOLE_SET_CURSOR_POS, DM_CONSOLE_SET_CURSOR_POS, x86_textmode_console_set_cursor_pos, 2)                                                                        \
    _X(CONSOLE_SET_CURSOR_VISIBLE, DM_CONSOLE_SET_CURSOR_VISIBLE, x86_textmode_console_set_cursor_visible, 1)

DECLARE_FUNCTION_ID_ENUM(console, CONSOLE_RPCS)
