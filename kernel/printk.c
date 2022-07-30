// SPDX-License-Identifier: GPL-3.0-or-later

#include "lib/stdio.h"
#include "lib/string.h"
#include "mos/device/console.h"
#include "mos/kernel.h"

#include <stdarg.h>

extern console_t *platform_consoles;

void lprintk(int loglevel, const char *fmt, ...)
{
    list_foreach(platform_consoles, console)
    {
        if (console->caps & CONSOLE_CAP_COLOR)
        {
            standard_color_t fg, bg;
            console->get_color(console, &fg, &bg);
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
            console->set_color(console, fg, bg);
        }
        char message[PRINTK_BUFFER_SIZE] = { 0 };
        va_list args;
        va_start(args, fmt);
        vsnprintf(message, PRINTK_BUFFER_SIZE, fmt, args);
        va_end(args);
        console->write(console, message, strlen(message));
    }
}
