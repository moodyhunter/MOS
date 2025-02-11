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
    explicit SerialConsole(const char *name, console_caps caps, Buffer<buf_size> *buffer, ISerialDevice *device, standard_color_t fg, standard_color_t bg)
        : Console(name, caps, buffer, fg, bg), device(device)
    {
    }

  public:
    void handle_irq();

  public:
    bool extra_setup() override;

    size_t do_write(const char *data, size_t size) override;

    bool set_color(standard_color_t fg, standard_color_t bg) override;

    bool clear() override;

    bool get_size(u32 *width, u32 *height) override;
};

bool serial_console_irq_handler(u32 irq, void *data);
