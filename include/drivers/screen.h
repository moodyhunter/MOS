// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "types.h"

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
} VGATextModeColor;

bool screen_init();
int screen_clear();

void screen_get_size(u32 *width, u32 *height);

void screen_print_char(char c);
bool screen_print_char_at(char c, u32 x, u32 y);

int screen_print_string(const char *str);
int screen_print_string_at(const char *str, u32 x, u32 y);

void screen_set_color(VGATextModeColor fg, VGATextModeColor bg);

bool screen_get_cursor_pos(u32 *x, u32 *y);
bool screen_set_cursor_pos(u32 x, u32 y);

void screen_enable_cursur(u8 cursor_start, u8 cursor_end);
void screen_disable_cursor();

void screen_scroll(void);
