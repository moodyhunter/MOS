// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <librpc/rpc.h>
#include <mos/device/dm_types.h>

#define X86_CONSOLE_SERVER_NAME "drivers.x86_text_console"

#define CONSOLE_RPCS_X(X, arg)                                                                                                                                           \
    X(arg, 1, write, WRITE, "s", ARG(const char *, str))                                                                                                                 \
    X(arg, 2, clear, CLEAR, "v")                                                                                                                                         \
    X(arg, 3, set_color, SET_COLOR, "ii", ARG(standard_color_t, fg), ARG(standard_color_t, bg))                                                                          \
    X(arg, 4, set_cursor_pos, SET_CURSOR_POS, "ii", ARG(u32, x), ARG(u32, y))                                                                                            \
    X(arg, 5, set_cursor_visibility, SET_CURSOR_VISIBILITY, "i", ARG(bool, visible))

RPC_DEFINE_ENUMS(console, CONSOLE, CONSOLE_RPCS_X)
