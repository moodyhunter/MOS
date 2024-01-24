// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#define SYSLOGD_RPC_X(ARGS, PB, arg)                                                                                                                                     \
    ARGS(arg, 0, set_name, SET_NAME, "s", ARG(STRING, name))                                                                                                             \
    ARGS(arg, 1, log, LOG, "s", ARG(STRING, message))                                                                                                                    \
    ARGS(arg, 2, logc, LOGC, "ss", ARG(STRING, category), ARG(STRING, message))

#define SYSLOGD_SERVICE_NAME "syslogd"

#ifdef __MOS_RPC_CLIENT__
#include <librpc/rpc_client.h>

#ifdef __MOS_MINIMAL_LIBC__
#include <mos_stdio.h>
#else
#include <stdio.h>
#endif

RPC_CLIENT_DEFINE_SIMPLECALL(syslogd, SYSLOGD_RPC_X)

should_inline void syslogd_logf(rpc_server_stub_t *logger, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    char buffer[1024];
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    syslogd_log(logger, buffer);
    va_end(args);
}
#endif
