// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "stdtypes.h"

typedef enum
{
    Black = 0x0,
    Blue = 0x1,
    Green = 0x2,
    Cyan = 0x3,
    Red = 0x4,
    Magenta = 0x5,
    Brown = 0x6,
    LightGray = 0x7,
    DarkGray = Black | 0x8,
    LightBlue = Blue | 0x8,
    LightGreen = Green | 0x8,
    LightCyan = Cyan | 0x8,
    LightRed = Red | 0x8,
    LightMagenta = Magenta | 0x8,
    Yellow = Brown | 0x8,
    White = LightGray | 0x8,
} TextModeColor;

int screen_init();
int screen_clear();

int screen_move_cursor(u32 x, u32 y);

int screen_get_cursor(u32 *x, u32 *y);
int screen_get_size(u32 *width, u32 *height);

int screen_print_char_at(u32 x, u32 y, char c, TextModeColor fg, TextModeColor bg);
int screen_print_string_at(u32 x, u32 y, const char *str, TextModeColor fg, TextModeColor bg);

int screen_print_string(const char *str);
int screen_print_string_colored(const char *str, TextModeColor fg, TextModeColor bg);

void screen_cursur_enable(u8 cursor_start, u8 cursor_end);
void screen_cursor_disable();
