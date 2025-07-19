// SPDX-License-Identifier: GPL-3.0-or-later

#include "syslogd.hpp"

#ifndef __MOS_KERNEL__
#include <errno.h>
#include <mos/syscall/usermode.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

__attribute__((__constructor__, __used__)) static void syslogd_init()
{
    fd_t fd = do_open_syslog_fd();
    if (fd < 0)
    {
        errno = -fd;
        perror("Failed to open syslog file descriptor");
        return;
    }

    // now redirect std::clog to syslog file descriptor
    const long ret = dup2(fd, STDERR_FILENO);
    if (ret < 0)
    {
        errno = -ret;
        perror("Failed to redirect stderr to syslog file descriptor");
        return;
    }
}

bool do_syslog(enum SyslogLevel level, const char *message)
{
    struct SyslogRequest request = {
        .level = level,
        .message = message,
        .messageSize = message ? strlen(message) : 0,
    };
    return syscall_kmod_call("syslogd", "log", &request, sizeof(request)) == 0;
}

fd_t do_open_syslog_fd()
{
    return syscall_kmod_call("syslogd", "open_syslogfd", NULL, 0);
}

#endif // __MOS_KERNEL__
