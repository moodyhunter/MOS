// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/syslog/debug.h"
#include "mos/syslog/syslog.h"

#include <mos/mos_global.h>
#include <mos/types.h>
#include <stdarg.h>

#ifndef pr_fmt
#define pr_fmt(fmt) fmt // default format string
#endif

#define emit_syslog(level, feat, fmt, ...)  do_syslog(level, current_thread, __FILE_NAME__, __func__, __LINE__, &mos_debug_info.feat, fmt, ##__VA_ARGS__)
#define emit_syslog_nofeat(level, fmt, ...) do_syslog(level, current_thread, __FILE_NAME__, __func__, __LINE__, NULL, fmt, ##__VA_ARGS__)

#define lprintk_debug_wrapper(feat, level, fmt, ...)                                                                                                                     \
    do                                                                                                                                                                   \
    {                                                                                                                                                                    \
        if (mos_debug_enabled(feat))                                                                                                                                     \
            emit_syslog(level, feat, fmt, ##__VA_ARGS__);                                                                                                                \
    } while (0)

// clang-format off
#define pr_dinfo2(feat, fmt, ...) lprintk_debug_wrapper(feat, MOS_LOG_INFO2, pr_fmt(fmt), ##__VA_ARGS__)
#define pr_dinfo(feat, fmt, ...)  lprintk_debug_wrapper(feat, MOS_LOG_INFO, pr_fmt(fmt), ##__VA_ARGS__)
#define pr_demph(feat, fmt, ...)  lprintk_debug_wrapper(feat, MOS_LOG_EMPH, pr_fmt(fmt), ##__VA_ARGS__)
#define pr_dwarn(feat, fmt, ...)  lprintk_debug_wrapper(feat, MOS_LOG_WARN, pr_fmt(fmt), ##__VA_ARGS__)
#define pr_demerg(feat, fmt, ...) lprintk_debug_wrapper(feat, MOS_LOG_EMERG, pr_fmt(fmt), ##__VA_ARGS__)
#define pr_dfatal(feat, fmt, ...) lprintk_debug_wrapper(feat, MOS_LOG_FATAL, pr_fmt(fmt), ##__VA_ARGS__)
#define pr_dcont(feat, fmt, ...)  do { if (mos_debug_enabled(feat)) pr_cont(fmt, ##__VA_ARGS__); } while (0)

#define pr_info(fmt, ...)  emit_syslog_nofeat(MOS_LOG_INFO, pr_fmt(fmt), ##__VA_ARGS__)
#define pr_info2(fmt, ...) emit_syslog_nofeat(MOS_LOG_INFO2, pr_fmt(fmt), ##__VA_ARGS__)
#define pr_emph(fmt, ...)  emit_syslog_nofeat(MOS_LOG_EMPH, pr_fmt(fmt), ##__VA_ARGS__)
#define pr_warn(fmt, ...)  emit_syslog_nofeat(MOS_LOG_WARN, pr_fmt(fmt), ##__VA_ARGS__)
#define pr_emerg(fmt, ...) emit_syslog_nofeat(MOS_LOG_EMERG, pr_fmt(fmt), ##__VA_ARGS__)
#define pr_fatal(fmt, ...) emit_syslog_nofeat(MOS_LOG_FATAL, pr_fmt(fmt), ##__VA_ARGS__)
#define pr_cont(fmt, ...)  emit_syslog_nofeat(MOS_LOG_UNSET, "" fmt, ##__VA_ARGS__)
// clang-format off

__BEGIN_DECLS

void lvprintk(loglevel_t loglevel, const char *fmt, va_list args);
__printf(1, 2) void printk(const char *format, ...);
__printf(2, 3) void lprintk(loglevel_t loglevel, const char *format, ...);

bool printk_unquiet(void);
void printk_set_quiet(bool quiet);

__END_DECLS
