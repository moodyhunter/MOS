// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/device/console.hpp"
#include "mos/device/serial.hpp"

#include <ansi_colors.h>
#include <stddef.h>

class SerialConsole : public Console
{
    ISerialDevice *device;

  public:
    template<size_t buf_size>
    explicit SerialConsole(const char *name, ConsoleCapFlags caps, Buffer<buf_size> *buffer, ISerialDevice *device, StandardColor fg, StandardColor bg)
        : Console(name, caps | CONSOLE_CAP_COLOR | CONSOLE_CAP_CLEAR, buffer, fg, bg), device(device)
    {
        device->setup();
    }

  public:
    void handle_irq();

  public:
    size_t do_write(const char *data, size_t size) override;

    bool set_color(StandardColor fg, StandardColor bg) override;

    bool clear() override;
};

bool serial_console_irq_handler(u32 irq, void *data);
