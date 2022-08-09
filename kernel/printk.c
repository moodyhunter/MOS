// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/printk.h"

#include "lib/containers.h"
#include "lib/stdio.h"
#include "lib/string.h"
#include "mos/device/console.h"

#include <stdarg.h>

static void deduce_level_color(int loglevel, standard_color_t *fg, standard_color_t *bg)
{
    *bg = Black;
    switch (loglevel)
    {
        case MOS_LOG_INFO2: *fg = DarkGray; break;
        case MOS_LOG_INFO: *fg = Gray; break;
        case MOS_LOG_EMPH: *fg = Cyan; break;
        case MOS_LOG_WARN: *fg = Brown; break;
        case MOS_LOG_EMERG: *fg = Red; break;
        case MOS_LOG_FATAL: *fg = White, *bg = Red; break;
        default: break; // do not change the color
    }
}

void lprintk(int loglevel, const char *fmt, ...)
{
    standard_color_t fg = White, bg = Black;
    deduce_level_color(loglevel, &fg, &bg);

    char message[PRINTK_BUFFER_SIZE] = { 0 };
    va_list args;
    va_start(args, fmt);
    vsnprintf(message, PRINTK_BUFFER_SIZE, fmt, args);
    va_end(args);
    const size_t len = strlen(message);

    standard_color_t prev_fg, prev_bg;
    list_foreach(console_t, console, consoles)
    {
        if (console->caps & CONSOLE_CAP_COLOR)
        {
            console->get_color(console, &prev_fg, &prev_bg);
            console->set_color(console, fg, bg);
        }

        console->write(console, message, len);

        if (console->caps & CONSOLE_CAP_COLOR)
        {
            console->set_color(console, prev_fg, prev_bg);
        }
    }
}
