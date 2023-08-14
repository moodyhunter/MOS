// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <mos/types.h>

typedef enum
{
    SIGINT = 2,   ///< interrupt
    SIGILL = 4,   ///< illegal instruction
    SIGTRAP = 5,  ///< trace/breakpoint trap
    SIGABRT = 6,  ///< abort
    SIGKILL = 9,  ///< kill
    SIGSEGV = 11, ///< invalid memory reference
    SIGTERM = 15, ///< termination
    SIGCHLD = 17, ///< child process terminated

    _SIGMAX_,
} signal_t;

#define SIGNAL_MAX_N 32
MOS_STATIC_ASSERT(SIGNAL_MAX_N > _SIGMAX_, "SIGNAL_MAX_N must be at least SIGSTOP + 1");

typedef void(sighandler)(signal_t signal);

typedef struct
{
    sighandler *handler;        // user-space signal handler
    void *sigreturn_trampoline; // trampoline code address when returning from a signal handler
} sigaction_t;

#define SIGACT_DEF ((sigaction_t) 0)
#define SIGACT_IGN ((sigaction_t) 1)
