// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <mos/mos_global.h>
#include <mos_string.h>

/**
 * @defgroup libs_ansicolors libs.AnsiColors
 * @ingroup libs
 * @brief ANSI color codes.
 * @{
 */

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

/**
 * @brief ANSI color codes creator.
 *
 * @details
 * Usage:
 * - a foreground color: ANSI_COLOR(foreground)
 * - a foreground color with style: ANSI_COLOR(foreground, style)
 * - a foreground color with style and a background color: ANSI_COLOR(foreground, style, background)
 */

// "_" is to prevent the "must specify at least one argument for '...' parameter of variadic macro" warning
#define ANSI_COLOR(...)  "\033[" _ANSI_COLOR_N(__VA_ARGS__, _ANSI_COLOR_3, _ANSI_COLOR_2, _ANSI_COLOR_1, _)(__VA_ARGS__) "m"
#define ANSI_COLOR_RESET "\033[0m"

// ! WARN: These below are not meant to be used directly

#define _ANSI_FG "3"
#define _ANSI_BG "4"

#define _ANSI_COLOR_black   "0"
#define _ANSI_COLOR_red     "1"
#define _ANSI_COLOR_green   "2"
#define _ANSI_COLOR_yellow  "3"
#define _ANSI_COLOR_blue    "4"
#define _ANSI_COLOR_magenta "5"
#define _ANSI_COLOR_cyan    "6"
#define _ANSI_COLOR_white   "7"

#define _ANSI_STYLE_regular    "0"
#define _ANSI_STYLE_bright     "1"
#define _ANSI_STYLE_faint      "2"
#define _ANSI_STYLE_italic     "3"
#define _ANSI_STYLE_underline  "4"
#define _ANSI_STYLE_blink      "5"
#define _ANSI_STYLE_blink_fast "6" // not widely supported
#define _ANSI_STYLE_reverse    "7"
#define _ANSI_STYLE_invisible  "8"

#define _ANSI_COLOR_1(fg)                 _ANSI_FG _ANSI_COLOR_##fg
#define _ANSI_COLOR_2(fg, style)          _ANSI_STYLE_##style ";" _ANSI_FG _ANSI_COLOR_##fg
#define _ANSI_COLOR_3(fg, style, bg)      _ANSI_STYLE_##style ";" _ANSI_FG _ANSI_COLOR_##fg ";" _ANSI_BG _ANSI_COLOR_##bg
#define _ANSI_COLOR_N(_1, _2, _3, N, ...) N

should_inline void get_ansi_color(char *buf, standard_color_t fg, standard_color_t bg)
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

    strcpy(buf, color);
}

/** @} */
