// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/riscv64/devices/sbi_console.hpp"

#include "mos/device/console.hpp"
#include "mos/riscv64/sbi/sbi-call.hpp"

#include <ansi_colors.h>
#include <mos/types.hpp>

static standard_color_t sbi_console_bg = Black, sbi_console_fg = White;

static size_t sbi_console_write(console_t *con, const char *data, size_t size)
{
    MOS_UNUSED(con);
    MOS_UNUSED(size);
    return sbi_putstring(data);
}

static bool sbi_console_set_color(console_t *con, standard_color_t fg, standard_color_t bg)
{
    MOS_UNUSED(con);
    sbi_console_fg = fg;
    sbi_console_bg = bg;
    char buf[64] = { 0 };
    get_ansi_color(buf, fg, bg);
    sbi_putstring(ANSI_COLOR_RESET);
    sbi_putstring(buf);
    return true;
}

static bool sbi_console_get_color(console_t *con, standard_color_t *fg, standard_color_t *bg)
{
    MOS_UNUSED(con);
    *fg = sbi_console_fg;
    *bg = sbi_console_bg;
    return true;
}

static bool sbi_console_clear(console_t *console)
{
    MOS_UNUSED(console);
    sbi_putstring("\033[2J");
    return true;
}

static console_ops_t sbi_console_ops = {
    .write = sbi_console_write,
    .get_color = sbi_console_get_color,
    .set_color = sbi_console_set_color,
    .clear = sbi_console_clear,
};

console_t sbi_console = {
    .ops = &sbi_console_ops,
    .name = "sbi-console",
    .caps = CONSOLE_CAP_COLOR | CONSOLE_CAP_CLEAR,
    .default_fg = White,
    .default_bg = Black,
};
