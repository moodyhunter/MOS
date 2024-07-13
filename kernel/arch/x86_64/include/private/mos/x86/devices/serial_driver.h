// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/device/serial.h"

typedef enum
{
    COM1 = 0x3F8,
    COM2 = 0x2F8,
    COM3 = 0x3E8,
    COM4 = 0x2E8,
    COM5 = 0x5F8,
    COM6 = 0x4F8,
    COM7 = 0x5E8,
    COM8 = 0x4E8
} x86_com_port_t;

extern const serial_driver_t x86_serial_driver;
