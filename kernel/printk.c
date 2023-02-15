// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/printk.h"

#include "lib/stdio.h"
#include "lib/string.h"
#include "lib/structures/list.h"
#include "lib/sync/spinlock.h"
#include "mos/cmdline.h"
#include "mos/device/console.h"
#include "mos/setup.h"

static console_t *printk_console;

static bool printk_setup_console(int argc, const char **argv)
{
    if (argc != 1)
    {
        pr_warn("printk_setup_console: expected 1 argument, got %d", argc);
        return false;
    }

    const char *const kcon_name = argv[0];

    console_t *console = console_get(kcon_name);
    if (console)
    {
        pr_emph("Selected console '%s' for future printk", kcon_name);
        printk_console = console;
        return true;
    }

    console = console_get_by_prefix(kcon_name);
    if (console)
    {
        pr_emph("Selected console '%s' for future printk (prefix-based)", console->name);
        printk_console = console;
        return true;
    }

    mos_warn("No console found for printk based on given name or prefix '%s'", kcon_name);
    printk_console = NULL;
    return false;
}

__setup("printk_console", printk_setup_console);

static inline void deduce_level_color(int loglevel, standard_color_t *fg, standard_color_t *bg)
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

static void print_to_console(console_t *con, int loglevel, const char *message, size_t len)
{
    if (!con)
        return;

    standard_color_t fg = White, bg = Black;
    deduce_level_color(loglevel, &fg, &bg);
    console_write_color(con, message, len, fg, bg);
}

static void lvprintk(mos_log_level_t loglevel, const char *fmt, va_list args)
{
    char message[PRINTK_BUFFER_SIZE] = { 0 };
    vsnprintf(message, PRINTK_BUFFER_SIZE, fmt, args);
    const size_t len = strlen(message);

    if (likely(printk_console))
    {
        print_to_console(printk_console, loglevel, message, len);
    }
    else
    {
        list_foreach(console_t, con, consoles)
        {
            print_to_console(con, loglevel, message, len);
        }
    }
}

void lprintk(mos_log_level_t loglevel, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    lvprintk(loglevel, format, args);
    va_end(args);
}

void printk(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    lvprintk(MOS_LOG_INFO, format, args);
    va_end(args);
}
