// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/syslog/debug.hpp"
#include "mos/syslog/syslog.hpp"

#include <mos/mos_global.h>
#include <mos/types.hpp>
#include <stdarg.h>

#ifndef pr_fmt
#define pr_fmt(fmt) fmt // default format string
#endif

#define emit_syslog(level, feat, fmt, ...)  do_syslog(LogLevel::level, __FILE_NAME__, __func__, __LINE__, &mos_debug_info.feat, fmt, ##__VA_ARGS__)
#define emit_syslog_nofeat(level, fmt, ...) do_syslog(LogLevel::level, __FILE_NAME__, __func__, __LINE__, NULL, fmt, ##__VA_ARGS__)

#define lprintk_debug_wrapper(feat, level, fmt, ...)                                                                                                                     \
    do                                                                                                                                                                   \
    {                                                                                                                                                                    \
        if (mos_debug_enabled(feat))                                                                                                                                     \
            emit_syslog(level, feat, fmt, ##__VA_ARGS__);                                                                                                                \
    } while (0)

// clang-format off
#define pr_dinfo2(feat, fmt, ...) lprintk_debug_wrapper(feat, INFO2, pr_fmt(fmt), ##__VA_ARGS__)
#define pr_dinfo(feat, fmt, ...)  lprintk_debug_wrapper(feat, INFO, pr_fmt(fmt), ##__VA_ARGS__)
#define pr_demph(feat, fmt, ...)  lprintk_debug_wrapper(feat, EMPH, pr_fmt(fmt), ##__VA_ARGS__)
#define pr_dwarn(feat, fmt, ...)  lprintk_debug_wrapper(feat, WARN, pr_fmt(fmt), ##__VA_ARGS__)
#define pr_demerg(feat, fmt, ...) lprintk_debug_wrapper(feat, EMERG, pr_fmt(fmt), ##__VA_ARGS__)
#define pr_dfatal(feat, fmt, ...) lprintk_debug_wrapper(feat, FATAL, pr_fmt(fmt), ##__VA_ARGS__)
#define pr_dcont(feat, fmt, ...)  do { if (mos_debug_enabled(feat)) pr_cont(fmt, ##__VA_ARGS__); } while (0)

#define pr_info(fmt, ...)  emit_syslog_nofeat(INFO, pr_fmt(fmt), ##__VA_ARGS__)
#define pr_info2(fmt, ...) emit_syslog_nofeat(INFO2, pr_fmt(fmt), ##__VA_ARGS__)
#define pr_emph(fmt, ...)  emit_syslog_nofeat(EMPH, pr_fmt(fmt), ##__VA_ARGS__)
#define pr_warn(fmt, ...)  emit_syslog_nofeat(WARN, pr_fmt(fmt), ##__VA_ARGS__)
#define pr_emerg(fmt, ...) emit_syslog_nofeat(EMERG, pr_fmt(fmt), ##__VA_ARGS__)
#define pr_fatal(fmt, ...) emit_syslog_nofeat(FATAL, pr_fmt(fmt), ##__VA_ARGS__)
#define pr_cont(fmt, ...)  emit_syslog_nofeat(UNSET, "" fmt, ##__VA_ARGS__)
// clang-format on

void print_to_console(Console *con, LogLevel loglevel, const char *message, size_t len);

void lvprintk(LogLevel loglevel, const char *fmt, va_list args);
__printf(1, 2) void printk(const char *format, ...);
__printf(2, 3) void lprintk(LogLevel loglevel, const char *format, ...);

bool printk_unquiet(void);
void printk_set_quiet(bool quiet);
