// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/syslog/debug.h"
#include "mos/syslog/printk_prefix.h"

#include <mos/mos_global.h>
#include <mos/types.h>

typedef enum
{
    MOS_LOG_FATAL = 6,
    MOS_LOG_EMERG = 5,
    MOS_LOG_WARN = 4,
    MOS_LOG_EMPH = 3,
    MOS_LOG_INFO = 2,
    MOS_LOG_INFO2 = 1,
    MOS_LOG_UNSET = 0,
} loglevel_t;

#ifndef pr_fmt
#define pr_fmt(fmt) fmt // default format string
#endif

#define lprintk_debug_wrapper(feat, level, fmt, ...)                                                                                                                     \
    do                                                                                                                                                                   \
    {                                                                                                                                                                    \
        if (mos_debug_enabled(feat))                                                                                                                                     \
            lprintk_wrapper(level, "%-10s | " fmt, #feat, ##__VA_ARGS__);                                                                                                \
    } while (0)

// clang-format off
#define pr_dinfo2(feat, fmt, ...) lprintk_debug_wrapper(feat, MOS_LOG_INFO2, pr_fmt(fmt), ##__VA_ARGS__)
#define pr_dinfo(feat, fmt, ...)  lprintk_debug_wrapper(feat, MOS_LOG_INFO, pr_fmt(fmt), ##__VA_ARGS__)
#define pr_demph(feat, fmt, ...)  lprintk_debug_wrapper(feat, MOS_LOG_EMPH, pr_fmt(fmt), ##__VA_ARGS__)
#define pr_dwarn(feat, fmt, ...)  lprintk_debug_wrapper(feat, MOS_LOG_WARN, pr_fmt(fmt), ##__VA_ARGS__)
#define pr_demerg(feat, fmt, ...) lprintk_debug_wrapper(feat, MOS_LOG_EMERG, pr_fmt(fmt), ##__VA_ARGS__)
#define pr_dfatal(feat, fmt, ...) lprintk_debug_wrapper(feat, MOS_LOG_FATAL, pr_fmt(fmt), ##__VA_ARGS__)
#define pr_dcont(feat, fmt, ...)  do { if (mos_debug_enabled(feat)) pr_cont(fmt, ##__VA_ARGS__); } while (0)

#define pr_info(fmt, ...)  lprintk_wrapper(MOS_LOG_INFO, pr_fmt(fmt), ##__VA_ARGS__)
#define pr_info2(fmt, ...) lprintk_wrapper(MOS_LOG_INFO2, pr_fmt(fmt), ##__VA_ARGS__)
#define pr_emph(fmt, ...)  lprintk_wrapper(MOS_LOG_EMPH, pr_fmt(fmt), ##__VA_ARGS__)
#define pr_warn(fmt, ...)  lprintk_wrapper(MOS_LOG_WARN, pr_fmt(fmt), ##__VA_ARGS__)
#define pr_emerg(fmt, ...) lprintk_wrapper(MOS_LOG_EMERG, pr_fmt(fmt), ##__VA_ARGS__)
#define pr_fatal(fmt, ...) lprintk_wrapper(MOS_LOG_FATAL, pr_fmt(fmt), ##__VA_ARGS__)
#define pr_cont(fmt, ...)  lprintk(MOS_LOG_UNSET, "" fmt, ##__VA_ARGS__)
// clang-format off

__BEGIN_DECLS

__printf(1, 2) void printk(const char *format, ...);
__printf(2, 3) void lprintk(loglevel_t loglevel, const char *format, ...);

bool printk_unquiet(void);
void printk_set_quiet(bool quiet);

__END_DECLS
