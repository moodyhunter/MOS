// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <mos/types.h>

#ifdef __cplusplus
#define ENUM_CLASS class
#else
#define ENUM_CLASS
#endif

enum ENUM_CLASS SyslogLevel
{
    Debug,
    Info,
    Notice,
    Warning,
    Error,
    Critical,
    Alert,
    Emergency
};

struct SyslogRequest
{
    enum SyslogLevel level; // Log level of the message
    const char *message;    // The log message to be processed
    size_t messageSize;     // Size of the log message
};

struct OpenReaderRequest
{
    fd_t fd; // File descriptor to read from
};

__maybe_unused static const char *SYSLOGD_MODULE_NAME = "syslogd";

#ifndef __MOS_KERNEL__
MOSAPI bool do_syslog(enum SyslogLevel level, const char *message);
MOSAPI fd_t do_open_syslog_fd();
#endif
