// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <mos/types.h>
#ifndef __MOS_KERNEL__
#include <mos/syscall/usermode.h>
#endif

static constexpr auto SYSLOGD_MODULE_NAME = "syslogd";

enum class SyslogLevel
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
    SyslogLevel level;   // Log level of the message
    const char *message; // The log message to be processed
    size_t messageSize;  // Size of the log message
};

struct OpenReaderRequest
{
    fd_t fd; // File descriptor to read from
};

#ifndef __MOS_KERNEL__
inline bool do_syslog(SyslogLevel level, const char *message)
{
    SyslogRequest request{ level, message };
    return syscall_kmod_call("syslogd", "log", &request, sizeof(request)) == 0;
}

inline fd_t do_open_syslog_fd()
{
    return syscall_kmod_call("syslogd", "open_syslogfd", NULL, 0);
}
#endif
