// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/device/dm_types.h"
#include "mos/types.h"

bool screen_get_size(u32 *width, u32 *height);
bool screen_get_cursor_pos(u32 *x, u32 *y);
bool screen_set_cursor_pos(u32 x, u32 y);
void screen_print_char(char c);
bool screen_enable_cursur(bool enable);
bool screen_get_color(standard_color_t *fg, standard_color_t *bg);
bool screen_set_color(standard_color_t fg, standard_color_t bg);
int screen_print_string(const char *str, size_t limit);
bool screen_clear(void);
void x86_vga_text_mode_console_init(uintptr_t video_buffer_addr);
