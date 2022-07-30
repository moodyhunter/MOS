// SPDX-Licence-Identifier: GPL-3.0-or-later

#pragma once

#define _ANSI_COLOR_FG_COLOR_black          ";30"
#define _ANSI_COLOR_FG_COLOR_red            ";31"
#define _ANSI_COLOR_FG_COLOR_green          ";32"
#define _ANSI_COLOR_FG_COLOR_yellow         ";33"
#define _ANSI_COLOR_FG_COLOR_blue           ";34"
#define _ANSI_COLOR_FG_COLOR_magenta        ";35"
#define _ANSI_COLOR_FG_COLOR_cyan           ";36"
#define _ANSI_COLOR_FG_COLOR_white          ";37"
#define _ANSI_COLOR_FG_COLOR_bright_black   ";90"
#define _ANSI_COLOR_FG_COLOR_bright_red     ";91"
#define _ANSI_COLOR_FG_COLOR_bright_green   ";92"
#define _ANSI_COLOR_FG_COLOR_bright_yellow  ";93"
#define _ANSI_COLOR_FG_COLOR_bright_blue    ";94"
#define _ANSI_COLOR_FG_COLOR_bright_magenta ";95"
#define _ANSI_COLOR_FG_COLOR_bright_cyan    ";96"
#define _ANSI_COLOR_FG_COLOR_bright_white   ";97"
#define _ANSI_COLOR_BG_COLOR_black          ";40"
#define _ANSI_COLOR_BG_COLOR_red            ";41"
#define _ANSI_COLOR_BG_COLOR_green          ";42"
#define _ANSI_COLOR_BG_COLOR_yellow         ";43"
#define _ANSI_COLOR_BG_COLOR_blue           ";44"
#define _ANSI_COLOR_BG_COLOR_magenta        ";45"
#define _ANSI_COLOR_BG_COLOR_cyan           ";46"
#define _ANSI_COLOR_BG_COLOR_white          ";47"
#define _ANSI_COLOR_BG_COLOR_bright_black   ";100"
#define _ANSI_COLOR_BG_COLOR_bright_red     ";101"
#define _ANSI_COLOR_BG_COLOR_bright_green   ";102"
#define _ANSI_COLOR_BG_COLOR_bright_yellow  ";103"
#define _ANSI_COLOR_BG_COLOR_bright_blue    ";104"
#define _ANSI_COLOR_BG_COLOR_bright_magenta ";105"
#define _ANSI_COLOR_BG_COLOR_bright_cyan    ";106"
#define _ANSI_COLOR_BG_COLOR_bright_white   ";107"

#define _ANSI_COLOR_STYLE_regular    "0"
#define _ANSI_COLOR_STYLE_bold       "1"
#define _ANSI_COLOR_STYLE_faint      "2"
#define _ANSI_COLOR_STYLE_italic     "3"
#define _ANSI_COLOR_STYLE_underline  "4"
#define _ANSI_COLOR_STYLE_blink      "5"
#define _ANSI_COLOR_STYLE_blink_fast "6" // not widely supported
#define _ANSI_COLOR_STYLE_reverse    "7"
#define _ANSI_COLOR_STYLE_invisible  "8"

#define _ANSI_COLOR_1(fg)                 _ANSI_COLOR_FG_COLOR_##fg
#define _ANSI_COLOR_2(fg, style)          _ANSI_COLOR_STYLE_##style _ANSI_COLOR_FG_COLOR_##fg
#define _ANSI_COLOR_3(fg, style, bg)      _ANSI_COLOR_STYLE_##style _ANSI_COLOR_FG_COLOR_##fg _ANSI_COLOR_BG_COLOR_##bg
#define _ANSI_COLOR_N(_1, _2, _3, N, ...) N

// "_" is to prevent "must specify at least one argument for '...' parameter of variadic macro" warning
#define ANSI_COLOR(...)  "\033[" _ANSI_COLOR_N(__VA_ARGS__, _ANSI_COLOR_3, _ANSI_COLOR_2, _ANSI_COLOR_1, _)(__VA_ARGS__) "m"
#define ANSI_COLOR_RESET "\e[0m"
