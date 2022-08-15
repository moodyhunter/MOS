// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/x86/drivers/serial_console.h"

#include "lib/ansi_colors.h"
#include "lib/string.h"
#include "mos/device/console.h"
#include "mos/x86/drivers/serial.h"

const char *ansi_reset = "0m";

bool serial_console_setup(console_t *console)
{
    console->write = serial_console_write;
    console->read = serial_console_read;
    console->caps |= CONSOLE_CAP_READ;
    if (console->caps & CONSOLE_CAP_COLOR)
    {
        console->set_color = serial_console_set_color;
        console->get_color = serial_console_get_color;
    }
    linked_list_init(list_node(console));
    serial_console_t *serial_console = container_of(console, serial_console_t, console);
    return serial_device_setup(&serial_console->device);
}

int serial_console_write(console_t *console, const char *str, size_t len)
{
    serial_console_t *serial_console = container_of(console, serial_console_t, console);
    return serial_device_write(&serial_console->device, str, len);
}

int serial_console_read(console_t *console, char *str, size_t len)
{
    serial_console_t *serial_console = container_of(console, serial_console_t, console);
    return serial_device_read(&serial_console->device, str, len);
}

void get_ansi_color(char *buf, standard_color_t fg, standard_color_t bg)
{
    MOS_UNUSED(bg);
    static const char *g_ansi_colors[] = {
        [Black] = "" ANSI_COLOR(black),
        [Blue] = "" ANSI_COLOR(blue),
        [Green] = "" ANSI_COLOR(green),
        [Cyan] = "" ANSI_COLOR(cyan),
        [Red] = "" ANSI_COLOR(red),
        [Magenta] = "" ANSI_COLOR(magenta),
        [Brown] = "" ANSI_COLOR(yellow),
        [Gray] = "" ANSI_COLOR(white),
        [DarkGray] = "" ANSI_COLOR(black, bright),
        [LightBlue] = "" ANSI_COLOR(blue, bright),
        [LightGreen] = "" ANSI_COLOR(green, bright),
        [LightCyan] = "" ANSI_COLOR(cyan, bright),
        [LightRed] = "" ANSI_COLOR(red, bright),
        [LightMagenta] = "" ANSI_COLOR(magenta, bright),
        [Yellow] = "" ANSI_COLOR(yellow, bright),
        [White] = "" ANSI_COLOR(white, bright),
    };

    strcat(buf, g_ansi_colors[fg]);
}

bool serial_console_set_color(console_t *device, standard_color_t fg, standard_color_t bg)
{
    serial_console_t *serial_console = container_of(device, serial_console_t, console);
    serial_console->fg = fg;
    serial_console->bg = bg;
    char buf[64] = { 0 };
    get_ansi_color(buf, fg, bg);
    serial_device_write(&serial_console->device, buf, strlen(buf));
    return true;
}

bool serial_console_get_color(console_t *device, standard_color_t *fg, standard_color_t *bg)
{
    serial_console_t *serial_console = container_of(device, serial_console_t, console);
    *fg = serial_console->fg;
    *bg = serial_console->bg;
    return true;
}
