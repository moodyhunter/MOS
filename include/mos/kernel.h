// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "mos/mos_global.h"
#include "mos/types.h"

#include <stdarg.h>

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

#define MOS_TODO(fmt, ...)         mos_warn("TODO: " fmt, ##__VA_ARGS__)
#define MOS_UNIMPLEMENTED(content) mos_panic("UNIMPLEMENTED: %s", content)
#define MOS_UNREACHABLE()          mos_panic("UNREACHABLE")

#define PRINTK_BUFFER_SIZE 1024

#define mos_lprintk(level, fmt, ...) lprintk(level, fmt "\n", ##__VA_ARGS__)

#define printk(fmt, ...) mos_lprintk(MOS_LOG_DEFAULT, fmt, ##__VA_ARGS__)

// print a colored message without handler, print unconditionally without a handler
#define pr_debug(fmt, ...) mos_lprintk(MOS_LOG_DEBUG, fmt, ##__VA_ARGS__)
#define pr_info(fmt, ...)  mos_lprintk(MOS_LOG_INFO, fmt, ##__VA_ARGS__)
#define pr_emph(fmt, ...)  mos_lprintk(MOS_LOG_EMPH, fmt, ##__VA_ARGS__)
#define pr_warn(fmt, ...)  mos_lprintk(MOS_LOG_WARN, fmt, ##__VA_ARGS__)
#define pr_emerg(fmt, ...) mos_lprintk(MOS_LOG_EMERG, fmt, ##__VA_ARGS__)
#define pr_fatal(fmt, ...) mos_lprintk(MOS_LOG_FATAL, fmt, ##__VA_ARGS__)

#define mos_warn(fmt, ...)  mos_kwarn(__func__, __LINE__, fmt "", ##__VA_ARGS__)
#define mos_panic(fmt, ...) mos_kpanic(__func__, __LINE__, fmt "", ##__VA_ARGS__)

void __attr_printf(2, 3) lprintk(int loglevel, const char *format, ...);
void __attr_printf(3, 4) mos_kwarn(const char *func, u32 line, const char *fmt, ...);
void __attr_noreturn __attr_printf(3, 4) mos_kpanic(const char *func, u32 line, const char *fmt, ...);

typedef struct
{
    void (*platform_init)(void);
    void __attr_noreturn (*platform_shutdown)(void);
    void (*enable_interrupts)(void);
    void (*disable_interrupts)(void);
} mos_platform_t;

extern mos_platform_t mos_platform;
