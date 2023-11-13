// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/printk_prefix_fmt.h"

#include <mos/mos_global.h>
#include <mos/types.h>
#include <stdarg.h>

#define PRINTK_BUFFER_SIZE 1024

typedef enum
{
    MOS_LOG_FATAL = 6,
    MOS_LOG_EMERG = 5,
    MOS_LOG_WARN = 4,
    MOS_LOG_EMPH = 3,
    MOS_LOG_INFO = 2,
    MOS_LOG_INFO2 = 1,
    MOS_LOG_UNSET = 0,
    MOS_LOG_DEFAULT = MOS_LOG_INFO,
} mos_loglevel;

#ifndef pr_fmt
#define pr_fmt(fmt) fmt // default format string
#endif

#define lprintk_debug_wrapper(feat, level, fmt, ...)                                                                                                                     \
    do                                                                                                                                                                   \
    {                                                                                                                                                                    \
        if (MOS_DEBUG_FEATURE(feat))                                                                                                                                     \
            lprintk_wrapper(level, "%-10s | " fmt, #feat, ##__VA_ARGS__);                                                                                                \
    } while (0)

#define pr_dinfo2(feat, fmt, ...) lprintk_debug_wrapper(feat, MOS_LOG_INFO2, pr_fmt(fmt), ##__VA_ARGS__)
#define pr_dinfo(feat, fmt, ...)  lprintk_debug_wrapper(feat, MOS_LOG_INFO, pr_fmt(fmt), ##__VA_ARGS__)
#define pr_demph(feat, fmt, ...)  lprintk_debug_wrapper(feat, MOS_LOG_EMPH, pr_fmt(fmt), ##__VA_ARGS__)
#define pr_dwarn(feat, fmt, ...)  lprintk_debug_wrapper(feat, MOS_LOG_WARN, pr_fmt(fmt), ##__VA_ARGS__)
#define pr_demerg(feat, fmt, ...) lprintk_debug_wrapper(feat, MOS_LOG_EMERG, pr_fmt(fmt), ##__VA_ARGS__)
#define pr_dfatal(feat, fmt, ...) lprintk_debug_wrapper(feat, MOS_LOG_FATAL, pr_fmt(fmt), ##__VA_ARGS__)
#define pr_dcont(feat, fmt, ...)                                                                                                                                         \
    do                                                                                                                                                                   \
    {                                                                                                                                                                    \
        if (MOS_DEBUG_FEATURE(feat))                                                                                                                                     \
            pr_cont(fmt, ##__VA_ARGS__);                                                                                                                                 \
    } while (0)

// print a colored message without handler, print unconditionally without a handler
#define pr_info(fmt, ...)  lprintk_wrapper(MOS_LOG_INFO, pr_fmt(fmt), ##__VA_ARGS__)
#define pr_info2(fmt, ...) lprintk_wrapper(MOS_LOG_INFO2, pr_fmt(fmt), ##__VA_ARGS__)
#define pr_emph(fmt, ...)  lprintk_wrapper(MOS_LOG_EMPH, pr_fmt(fmt), ##__VA_ARGS__)
#define pr_warn(fmt, ...)  lprintk_wrapper(MOS_LOG_WARN, pr_fmt(fmt), ##__VA_ARGS__)
#define pr_emerg(fmt, ...) lprintk_wrapper(MOS_LOG_EMERG, pr_fmt(fmt), ##__VA_ARGS__)
#define pr_fatal(fmt, ...) lprintk_wrapper(MOS_LOG_FATAL, pr_fmt(fmt), ##__VA_ARGS__)
#define pr_cont(fmt, ...)  lprintk(MOS_LOG_UNSET, "" fmt, ##__VA_ARGS__)

__printf(1, 2) void printk(const char *format, ...);
__printf(2, 3) void lprintk(mos_loglevel loglevel, const char *format, ...);

void vprintk(const char *format, va_list args);
void lvprintk(mos_loglevel loglevel, const char *fmt, va_list args);

bool printk_unquiet(void);
void printk_set_quiet(bool quiet);
