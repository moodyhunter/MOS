// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

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

// USAGE:
//
// ANSI_COLOR(foreground)                       // a foreground color
// ANSI_COLOR(foreground, style)                // a foreground color with style
// ANSI_COLOR(foreground, style, background)    // a foreground color with style and a background color
//
// "_" is to prevent the "must specify at least one argument for '...' parameter of variadic macro" warning
#define ANSI_COLOR(...)  "\033[" _ANSI_COLOR_N(__VA_ARGS__, _ANSI_COLOR_3, _ANSI_COLOR_2, _ANSI_COLOR_1, _)(__VA_ARGS__) "m"
#define ANSI_COLOR_RESET "\033[0m"
