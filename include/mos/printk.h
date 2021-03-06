// SPDX-Licence-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/mos_global.h"
#include "mos/types.h"

#define PRINTK_BUFFER_SIZE 1024

#define MOS_TODO(fmt, ...)         mos_warn("TODO: " fmt, ##__VA_ARGS__)
#define MOS_UNIMPLEMENTED(content) mos_panic("UNIMPLEMENTED: %s", content)
#define MOS_UNREACHABLE()          mos_panic("UNREACHABLE")
#define MOS_ASSERT(cond)                                                                                                                        \
    do                                                                                                                                          \
    {                                                                                                                                           \
        if (!(cond))                                                                                                                            \
            mos_panic("Assertion failed: " #cond);                                                                                              \
    } while (0)

#define MOS_LOG_FATAL 0
#define MOS_LOG_EMERG 1
#define MOS_LOG_WARN  2
#define MOS_LOG_EMPH  3
#define MOS_LOG_INFO  4
#define MOS_LOG_DEBUG 5

#define MOS_LOG_DEFAULT MOS_LOG_INFO

// print a colored message without handler, print unconditionally without a handler
#define pr_debug(fmt, ...) mos_lprintk(MOS_LOG_DEBUG, fmt, ##__VA_ARGS__)
#define pr_info(fmt, ...)  mos_lprintk(MOS_LOG_INFO, fmt, ##__VA_ARGS__)
#define pr_emph(fmt, ...)  mos_lprintk(MOS_LOG_EMPH, fmt, ##__VA_ARGS__)
#define pr_warn(fmt, ...)  mos_lprintk(MOS_LOG_WARN, fmt, ##__VA_ARGS__)
#define pr_emerg(fmt, ...) mos_lprintk(MOS_LOG_EMERG, fmt, ##__VA_ARGS__)
#define pr_fatal(fmt, ...) mos_lprintk(MOS_LOG_FATAL, fmt, ##__VA_ARGS__)

#define mos_warn(fmt, ...)           mos_kwarn(__func__, __LINE__, fmt "", ##__VA_ARGS__)
#define mos_panic(fmt, ...)          mos_kpanic(__func__, __LINE__, fmt "", ##__VA_ARGS__)
#define mos_lprintk(level, fmt, ...) lprintk(level, fmt "\n", ##__VA_ARGS__)
#define printk(fmt, ...)             mos_lprintk(MOS_LOG_DEFAULT, fmt, ##__VA_ARGS__)

void __printf(2, 3) lprintk(int loglevel, const char *format, ...);
void __printf(3, 4) mos_kwarn(const char *func, u32 line, const char *fmt, ...);
void __noreturn __printf(3, 4) mos_kpanic(const char *func, u32 line, const char *fmt, ...);
