// SPDX-License-Identifier: GPL-3.0-or-later

#include "ansi_colors.h"

#include <mos/device/console.h>
#include <mos/lib/structures/list.h>
#include <mos/x86/devices/serial.h>
#include <mos/x86/devices/serial_console.h>
#include <mos_string.h>

const char ansi_reset[] = ANSI_COLOR_RESET;

bool serial_console_setup(console_t *console)
{
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
    linked_list_init(list_node(console));
    serial_console_t *serial_con = container_of(console, serial_console_t, con);
    return serial_device_setup(&serial_con->device);
}

size_t serial_console_write(console_t *console, const char *str, size_t len)
{
    serial_console_t *serial_con = container_of(console, serial_console_t, con);
    return serial_device_write(&serial_con->device, str, len);
}

bool serial_console_set_color(console_t *console, standard_color_t fg, standard_color_t bg)
{
    serial_console_t *serial_con = container_of(console, serial_console_t, con);
    serial_con->fg = fg;
    serial_con->bg = bg;
    char buf[64] = { 0 };
    get_ansi_color(buf, fg, bg);
    serial_device_write(&serial_con->device, ansi_reset, sizeof(ansi_reset) - 1);
    serial_device_write(&serial_con->device, buf, strlen(buf));
    return true;
}

bool serial_console_get_color(console_t *console, standard_color_t *fg, standard_color_t *bg)
{
    serial_console_t *serial_con = container_of(console, serial_console_t, con);
    *fg = serial_con->fg;
    *bg = serial_con->bg;
    return true;
}

bool serial_console_clear(console_t *console)
{
    serial_console_t *serial_con = container_of(console, serial_console_t, con);
    serial_device_write(&serial_con->device, "\033[2J", 4);
    return true;
}
