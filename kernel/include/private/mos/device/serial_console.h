// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/device/console.h"
#include "mos/device/serial.h"

#include <ansi_colors.h>
#include <stddef.h>

typedef struct
{
    console_t con;
    serial_device_t device;
    standard_color_t fg, bg;
} serial_console_t;

MOS_STATIC_ASSERT(offsetof(serial_console_t, con) == 0, "console must be the first field in serial_console_t");

bool serial_console_setup(console_t *console);

bool serial_console_irq_handler(u32 irq, void *data);
