// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <mos/kconfig.h>
#include <mos/mos_global.h>
#include <mos/types.h>

#define PRINTK_BUFFER_SIZE 1024

#define MOS_UNIMPLEMENTED(content)  mos_panic("\nUNIMPLEMENTED: %s", content)
#define MOS_UNREACHABLE()           mos_panic("\nUNREACHABLE line %d reached in file: %s", __LINE__, __FILE__)
#define MOS_UNREACHABLE_X(msg, ...) mos_panic("\nUNREACHABLE line %d reached in file: %s\n" msg, __LINE__, __FILE__, ##__VA_ARGS__)
#define MOS_ASSERT_ONCE(...)        MOS_ASSERT_X(once(), __VA_ARGS__)
#define MOS_ASSERT(cond)            MOS_ASSERT_X(cond, "")
#define MOS_ASSERT_X(cond, msg, ...)                                                                                                                                     \
    do                                                                                                                                                                   \
    {                                                                                                                                                                    \
        if (unlikely(!(cond)))                                                                                                                                           \
            mos_panic("Assertion failed: %s \n" msg, #cond, ##__VA_ARGS__);                                                                                              \
    } while (0)

typedef enum
{
    MOS_LOG_FATAL = 6,
    MOS_LOG_EMERG = 5,
    MOS_LOG_WARN = 4,
    MOS_LOG_EMPH = 3,
    MOS_LOG_INFO = 2,
    MOS_LOG_INFO2 = 1,
    MOS_LOG_DEFAULT = MOS_LOG_INFO,
} mos_log_level_t;

#if MOS_CONFIG(MOS_DEBUG_HAS_FUNCTION_NAME)
#define MOS_FUNCTION_NAME __func__
#else
#define MOS_FUNCTION_NAME ""
#endif

#define mos_debug(feat, fmt, ...)                                                                                                                                        \
    do                                                                                                                                                                   \
    {                                                                                                                                                                    \
        if (MOS_DEBUG_FEATURE(feat))                                                                                                                                     \
            pr_info2("%-15s %-25s: " fmt, #feat, MOS_FUNCTION_NAME, ##__VA_ARGS__);                                                                                      \
    } while (0)

#if MOS_CONFIG(MOS_PRINTK_WITH_FILENAME)
#define lprintk_wrapper(level, fmt, ...) lprintk(level, "%-30s | " fmt "\r\n", MOS_FILE_LOCATION, ##__VA_ARGS__)
#else
#define lprintk_wrapper(level, fmt, ...) lprintk(level, fmt "\r\n", ##__VA_ARGS__)
#endif

// print a colored message without handler, print unconditionally without a handler
#define pr_info(fmt, ...)  lprintk_wrapper(MOS_LOG_INFO, fmt, ##__VA_ARGS__)
#define pr_info2(fmt, ...) lprintk_wrapper(MOS_LOG_INFO2, fmt, ##__VA_ARGS__)
#define pr_emph(fmt, ...)  lprintk_wrapper(MOS_LOG_EMPH, fmt, ##__VA_ARGS__)
#define pr_warn(fmt, ...)  lprintk_wrapper(MOS_LOG_WARN, fmt, ##__VA_ARGS__)
#define pr_emerg(fmt, ...) lprintk_wrapper(MOS_LOG_EMERG, fmt, ##__VA_ARGS__)
#define pr_fatal(fmt, ...) lprintk_wrapper(MOS_LOG_FATAL, fmt, ##__VA_ARGS__)

// these two also invokes a warning/panic handler
#define mos_warn(fmt, ...)  mos_kwarn(__func__, __LINE__, "WARN: " fmt "\r\n", ##__VA_ARGS__)
#define mos_panic(fmt, ...) mos_kpanic(__func__, __LINE__, "" fmt, ##__VA_ARGS__)

#define mos_warn_once(...)                                                                                                                                               \
    do                                                                                                                                                                   \
    {                                                                                                                                                                    \
        if (once())                                                                                                                                                      \
            mos_warn(__VA_ARGS__);                                                                                                                                       \
    } while (0)

__printf(1, 2) void printk(const char *format, ...);
__printf(2, 3) void lprintk(mos_log_level_t loglevel, const char *format, ...);
__printf(3, 4) void mos_kwarn(const char *func, u32 line, const char *fmt, ...);
noreturn __printf(3, 4) void mos_kpanic(const char *func, u32 line, const char *fmt, ...);

bool printk_unquiet(void);
void printk_set_quiet(bool quiet);
