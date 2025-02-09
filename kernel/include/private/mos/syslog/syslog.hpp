// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/syslog/debug.hpp"

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

long do_syslog(loglevel_t level, const char *file, const char *func, int line, const debug_info_entry *feat, const char *fmt, ...);
