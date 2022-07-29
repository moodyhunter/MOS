// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/kernel.h"
#include "mos/mos_global.h"
#include "mos/stdio.h"
#include "mos/x86/drivers/screen.h"

#include <stdarg.h>

void lprintk(int loglevel, const char *fmt, ...)
{
    VGATextModeColor prev_fg;
    VGATextModeColor prev_bg;
    screen_get_color(&prev_fg, &prev_bg);

    VGATextModeColor fg = prev_fg;
    VGATextModeColor bg = Black;

    switch (loglevel)
    {
        case MOS_LOG_DEBUG: fg = DarkGray; break;
        case MOS_LOG_INFO: fg = LightGray; break;
        case MOS_LOG_EMPH: fg = Cyan; break;
        case MOS_LOG_WARN: fg = Brown; break;
        case MOS_LOG_EMERG: fg = Red; break;
        case MOS_LOG_FATAL: fg = White, bg = Red; break;
        default: break; // do not change the color
    }
    screen_set_color(fg, bg);

    char message[PRINTK_BUFFER_SIZE] = { 0 };
    va_list args;
    va_start(args, fmt);
    vsnprintf(message, PRINTK_BUFFER_SIZE, fmt, args);
    va_end(args);
    screen_print_string(message);
    screen_set_color(prev_fg, prev_bg);
}
