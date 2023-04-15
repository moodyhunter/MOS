// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "common.h"

#include <mos/device/dm_types.h>
#include <mos/mos_global.h>

void open_console(void);
__printf(1, 2) void print_to_console(const char *fmt, ...);
void set_console_color(standard_color_t fg, standard_color_t bg);
