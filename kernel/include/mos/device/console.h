// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "lib/containers.h"
#include "mos/types.h"

#define STD_COLOR_LIGHT 0x8

typedef enum
{
    Black = 0x0,
    Blue = 0x1,
    Green = 0x2,
    Cyan = 0x3,
    Red = 0x4,
    Magenta = 0x5,
    Brown = 0x6,
    Gray = 0x7,
    DarkGray = Black | STD_COLOR_LIGHT,
    LightBlue = Blue | STD_COLOR_LIGHT,
    LightGreen = Green | STD_COLOR_LIGHT,
    LightCyan = Cyan | STD_COLOR_LIGHT,
    LightRed = Red | STD_COLOR_LIGHT,
    LightMagenta = Magenta | STD_COLOR_LIGHT,
    Yellow = Brown | STD_COLOR_LIGHT,
    White = Gray | STD_COLOR_LIGHT,
} standard_color_t;

typedef enum
{
    CONSOLE_CAP_NONE = 0,
    CONSOLE_CAP_COLOR = 1 << 0,
    CONSOLE_CAP_CLEAR = 1 << 1,
    CONSOLE_CAP_READ = 1 << 2,
    CONSOLE_CAP_SETUP = 1 << 3,
    CONSOLE_CAP_GET_SIZE = 1 << 4,
    CONSOLE_CAP_CURSOR_HIDE = 1 << 5,
    CONSOLE_CAP_CURSOR_MOVE = 1 << 6,
} console_caps_t;

typedef struct console_t console_t;

struct console_t
{
    as_linked_list;

    const char *name;
    console_caps_t caps;

    bool (*setup)(console_t *con);
    bool (*get_size)(console_t *con, u32 *width, u32 *height);

    bool (*set_cursor)(console_t *con, bool show);
    bool (*move_cursor)(console_t *con, u32 x, u32 y);
    bool (*get_cursor)(console_t *con, u32 *x, u32 *y);

    // VGA standard color codes
    bool (*get_color)(console_t *con, standard_color_t *fg, standard_color_t *bg);
    bool (*set_color)(console_t *con, standard_color_t fg, standard_color_t bg);

    // not meant to be called directly, use console_read / console_write instead
    int (*read_impl)(console_t *con, char *dest, size_t size);
    int (*write_impl)(console_t *con, const char *data, size_t size);

    bool (*clear)(console_t *con);
    bool (*close)(console_t *con);

    void *data;
};

extern list_node_t consoles;
void console_register(console_t *con);
console_t *console_get(const char *name);

int console_read(console_t *con, char *dest, size_t size);
int console_write(console_t *con, const char *data, size_t size);
