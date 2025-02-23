// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/syslog/syslog.hpp"

#include <mos/device/console.hpp>
#include <mos/lib/structures/list.hpp>
#include <mos/lib/sync/spinlock.hpp>
#include <mos/misc/cmdline.hpp>
#include <mos/misc/setup.hpp>
#include <mos/syslog/printk.hpp>
#include <mos_stdio.hpp>
#include <mos_string.hpp>

Console *printk_console = NULL;
static bool printk_quiet = false;

MOS_SETUP("printk_console", printk_setup_console)
{
    const auto kcon_name = arg;
    if (kcon_name.empty())
    {
        pr_warn("No console name given for printk");
        return false;
    }

    Console *console = console_get(kcon_name);
    if (console)
    {
        pr_emph("Selected console '%s' for future printk\n", kcon_name);
        printk_console = console;
        return true;
    }

    console = console_get_by_prefix(kcon_name);
    if (console)
    {
        pr_emph("Selected console '%s' for future printk (prefix-based)\n", console->name);
        printk_console = console;
        return true;
    }

    mos_warn("No console found for printk based on given name or prefix '%s'", kcon_name.data());
    printk_console = NULL;
    return false;
}

MOS_EARLY_SETUP("quiet", printk_setup_quiet)
{
    printk_quiet = cmdline_string_truthiness(arg, true);
    return true;
}

static inline void deduce_level_color(LogLevel loglevel, standard_color_t *fg, standard_color_t *bg)
{
    *bg = Black;
    switch (loglevel)
    {
        case LogLevel::INFO2: *fg = DarkGray; break;
        case LogLevel::INFO: *fg = Gray; break;
        case LogLevel::EMPH: *fg = Cyan; break;
        case LogLevel::WARN: *fg = Brown; break;
        case LogLevel::EMERG: *fg = Red; break;
        case LogLevel::FATAL: *fg = White, *bg = Red; break;
        default: break; // do not change the color
    }
}

void print_to_console(Console *con, LogLevel loglevel, const char *message, size_t len)
{
    if (!con)
        return;

    standard_color_t fg = con->fg, bg = con->bg;
    deduce_level_color(loglevel, &fg, &bg);
    con->WriteColored(message, len, fg, bg);
}

void lvprintk(LogLevel loglevel, const char *fmt, va_list args)
{
    // only print warnings and errors if quiet mode is enabled
    if (printk_quiet && loglevel < LogLevel::WARN)
        return;

    char message[MOS_PRINTK_BUFFER_SIZE];
    const int len = vsnprintf(message, MOS_PRINTK_BUFFER_SIZE, fmt, args);

    if (unlikely(!printk_console))
        printk_console = consoles.front();
    print_to_console(printk_console, loglevel, message, len);
}

bool printk_unquiet(void)
{
    bool was_quiet = printk_quiet;
    printk_quiet = false;
    return was_quiet;
}

void printk_set_quiet(bool quiet)
{
    printk_quiet = quiet;
}

void lprintk(LogLevel loglevel, const char *format, ...)
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
    lvprintk(LogLevel::INFO, format, args);
    va_end(args);
}
