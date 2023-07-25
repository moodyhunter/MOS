// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <mos/device/console.h>
#include <mos/x86/devices/serial.h>

typedef struct
{
    console_t con;
    serial_device_t *device;
    standard_color_t fg, bg;
} serial_console_t;

bool serial_console_setup(console_t *console);
size_t serial_console_write(console_t *console, const char *str, size_t len);
bool serial_console_set_color(console_t *device, standard_color_t fg, standard_color_t bg);
bool serial_console_get_color(console_t *device, standard_color_t *fg, standard_color_t *bg);
bool serial_console_clear(console_t *device);
