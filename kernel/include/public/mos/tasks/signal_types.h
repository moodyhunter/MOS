// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <mos/types.h>
#include <signal.h>

#define SIGNAL_MAX_N NSIG
typedef int signal_t;

typedef void(sighandler)(signal_t signal);

typedef struct
{
    sighandler *handler;        // user-space signal handler
    void *sigreturn_trampoline; // trampoline code address when returning from a signal handler
} sigaction_t;

#define SIGACT_DEF ((sigaction_t) 0)
#define SIGACT_IGN ((sigaction_t) 1)
