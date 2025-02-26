// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/device/serial_console.hpp"

#include "ansi_colors.h"
#include "mos/device/console.hpp"

#include <mos/lib/structures/list.hpp>
#include <mos_stdio.hpp>
#include <mos_string.hpp>

bool serial_console_irq_handler(u32 irq, void *data)
{
    MOS_UNUSED(irq);

    Console *const console = (Console *) data;
    SerialConsole *const serial_con = static_cast<SerialConsole *>(console);
    serial_con->handle_irq();

    return true;
}

size_t SerialConsole::do_write(const char *data, size_t size)
{
    return device->write_data(data, size);
}

bool SerialConsole::set_color(StandardColor fg, StandardColor bg)
{
    this->fg = fg;
    this->bg = bg;
    char buf[64] = { 0 };
    get_ansi_color(buf, fg, bg);
    device->write_data(ANSI_COLOR_RESET, sizeof(ANSI_COLOR_RESET) - 1);
    device->write_data(buf, strlen(buf));
    return true;
}

bool SerialConsole::clear()
{
    device->write_data("\033[2J", 4);
    return true;
}

void SerialConsole::handle_irq()
{
    while (device->get_data_ready())
    {
        char c = device->ReadByte();
        if (c == '\r')
            c = '\n';
        device->WriteByte(c);
        this->putc(c);
    }
};
