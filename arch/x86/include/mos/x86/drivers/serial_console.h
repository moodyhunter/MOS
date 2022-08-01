// SPDX-Licence-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/device/console.h"
#include "mos/x86/drivers/serial.h"

typedef struct
{
    serial_device_t device;
    console_t console;
} serial_console_t;

bool serial_console_setup(console_t *console);
int serial_console_read(console_t *console, char *str, size_t len);
int serial_console_write(console_t *console, const char *str, size_t len);
