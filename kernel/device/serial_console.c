// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/device/serial_console.h"

#include "ansi_colors.h"
#include "mos/device/console.h"

#include <mos/lib/structures/list.h>
#include <mos_string.h>

static size_t serial_console_write(console_t *console, const char *str, size_t len)
{
    serial_console_t *serial_con = container_of(console, serial_console_t, con);
    return serial_device_write(&serial_con->device, str, len);
}

static bool serial_console_set_color(console_t *console, standard_color_t fg, standard_color_t bg)
{
    serial_console_t *serial_con = container_of(console, serial_console_t, con);
    serial_con->fg = fg;
    serial_con->bg = bg;
    char buf[64] = { 0 };
    get_ansi_color(buf, fg, bg);
    serial_device_write(&serial_con->device, ANSI_COLOR_RESET, sizeof(ANSI_COLOR_RESET) - 1);
    serial_device_write(&serial_con->device, buf, strlen(buf));
    return true;
}

static bool serial_console_get_color(console_t *console, standard_color_t *fg, standard_color_t *bg)
{
    serial_console_t *serial_con = container_of(console, serial_console_t, con);
    *fg = serial_con->fg;
    *bg = serial_con->bg;
    return true;
}

static bool serial_console_clear(console_t *console)
{
    serial_console_t *serial_con = container_of(console, serial_console_t, con);
    serial_device_write(&serial_con->device, "\033[2J", 4);
    return true;
}

bool serial_console_setup(console_t *console)
{
    linked_list_init(list_node(console));

    serial_console_t *const serial_con = container_of(console, serial_console_t, con);

    if (!console->ops->write)
        console->ops->write = serial_console_write;

    console->caps |= CONSOLE_CAP_COLOR;
    if (!console->ops->set_color)
        console->ops->set_color = serial_console_set_color;

    if (!console->ops->get_color)
        console->ops->get_color = serial_console_get_color;

    console->caps |= CONSOLE_CAP_CLEAR;
    if (!console->ops->clear)
        console->ops->clear = serial_console_clear;

    return serial_device_setup(&serial_con->device);
}

bool serial_console_irq_handler(u32 irq, void *data)
{
    MOS_UNUSED(irq);

    console_t *const console = (console_t *) data;
    serial_console_t *const serial_con = container_of(console, serial_console_t, con);

    while (serial_dev_get_data_ready(&serial_con->device))
    {
        char c = '\0';
        serial_device_read(&serial_con->device, &c, 1);
        if (c == '\r')
            c = '\n';
        serial_device_write(&serial_con->device, &c, 1);
        console_putc(&serial_con->con, c);
    }

    return true;
}
