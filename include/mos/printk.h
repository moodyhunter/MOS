// SPDX-Licence-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/mos_global.h"
#include "mos/types.h"

#define PRINTK_BUFFER_SIZE 1024

#define B
#define KB B * 1024
#define MB KB * 1024
#define GB MB * 1024

#define MOS_TODO(fmt, ...)         mos_warn("TODO: " fmt, ##__VA_ARGS__)
#define MOS_UNIMPLEMENTED(content) mos_panic("UNIMPLEMENTED: %s", content)
#define MOS_UNREACHABLE()          mos_panic("UNREACHABLE")
#define MOS_ASSERT(cond)           MOS_ASSERT_X(cond, "")
#define MOS_ASSERT_X(cond, msg)                                                                                                                 \
    do                                                                                                                                          \
    {                                                                                                                                           \
        if (!(cond))                                                                                                                            \
            mos_panic("Assertion failed: %s %s", #cond, "\n" msg);                                                                              \
    } while (0)

#define MOS_LOG_FATAL 0
#define MOS_LOG_EMERG 1
#define MOS_LOG_WARN  2
#define MOS_LOG_EMPH  3
#define MOS_LOG_INFO  4
#define MOS_LOG_INFO2 5

#define MOS_LOG_DEFAULT MOS_LOG_INFO

// print a colored message without handler, print unconditionally without a handler
#define pr_info(fmt, ...)  lprintk(MOS_LOG_INFO, fmt "\n", ##__VA_ARGS__)
#define pr_info2(fmt, ...) lprintk(MOS_LOG_INFO2, fmt "\n", ##__VA_ARGS__)
#define pr_emph(fmt, ...)  lprintk(MOS_LOG_EMPH, fmt "\n", ##__VA_ARGS__)
#define pr_warn(fmt, ...)  lprintk(MOS_LOG_WARN, fmt "\n", ##__VA_ARGS__)
#define pr_emerg(fmt, ...) lprintk(MOS_LOG_EMERG, fmt "\n", ##__VA_ARGS__)
#define pr_fatal(fmt, ...) lprintk(MOS_LOG_FATAL, fmt "\n", ##__VA_ARGS__)
#define printk(fmt, ...)   lprintk(MOS_LOG_DEFAULT, fmt "", ##__VA_ARGS__)

#if MOS_ENABLE_DEBUG_LOG
#define mos_debug(fmt, ...) pr_info2("" fmt, ##__VA_ARGS__)
#else
#define mos_debug(fmt, ...) lprintk_noop(MOS_LOG_INFO2, fmt "\n", ##__VA_ARGS__)
#endif
#define mos_warn(fmt, ...)  mos_kwarn(__func__, __LINE__, "WARN: " fmt, ##__VA_ARGS__)
#define mos_panic(fmt, ...) mos_kpanic(__func__, __LINE__, "" fmt, ##__VA_ARGS__)

#define mos_warn_once(...)                                                                                                                      \
    do                                                                                                                                          \
    {                                                                                                                                           \
        static bool __once = false;                                                                                                             \
        if (!__once)                                                                                                                            \
        {                                                                                                                                       \
            __once = true;                                                                                                                      \
            mos_warn(__VA_ARGS__);                                                                                                              \
        }                                                                                                                                       \
    } while (0)

#define lprintk_noop(level, fmt, ...)                                                                                                           \
    do                                                                                                                                          \
    {                                                                                                                                           \
        if (0)                                                                                                                                  \
            lprintk(level, fmt "", ##__VA_ARGS__);                                                                                              \
    } while (0)

void __printf(2, 3) lprintk(int loglevel, const char *format, ...);
void __printf(3, 4) mos_kwarn(const char *func, u32 line, const char *fmt, ...);
void __printf(3, 4) __noreturn mos_kpanic(const char *func, u32 line, const char *fmt, ...);
