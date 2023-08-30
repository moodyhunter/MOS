// SPDX-License-Identifier: GPL-3.0-or-later

#include "ansi_colors.h"

#include <mos/device/console.h>
#include <mos/lib/structures/list.h>
#include <mos/x86/devices/serial.h>
#include <mos/x86/devices/serial_console.h>
#include <string.h>

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

void get_ansi_color(char *buf, standard_color_t fg, standard_color_t bg)
{
    MOS_UNUSED(bg);
    static const char *g_ansi_colors[] = {
        [Black] = ANSI_COLOR(black),
        [Blue] = ANSI_COLOR(blue),
        [Green] = ANSI_COLOR(green),
        [Cyan] = ANSI_COLOR(cyan),
        [Red] = ANSI_COLOR(red),
        [Magenta] = ANSI_COLOR(magenta),
        [Brown] = ANSI_COLOR(yellow),
        [Gray] = ANSI_COLOR(white, bright),
        [DarkGray] = ANSI_COLOR(white),
        [LightBlue] = ANSI_COLOR(blue, bright),
        [LightGreen] = ANSI_COLOR(green, bright),
        [LightCyan] = ANSI_COLOR(cyan, bright),
        [LightRed] = ANSI_COLOR(red, bright),
        [LightMagenta] = ANSI_COLOR(magenta, bright),
        [Yellow] = ANSI_COLOR(yellow, bright),
        [White] = ANSI_COLOR(white, bright),
    };

    const char *color = g_ansi_colors[fg];

    // TODO: add support for background colors
    if (bg == Red)
        color = ANSI_COLOR(red, blink);

    strcat(buf, color);
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
