// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <abi-bits/signal.h>
#include <mos/mos_global.h>

#define SIGNAL_MAX_N NSIG
typedef int signal_t;
typedef struct
{
    __sighandler handler;
    unsigned long sa_flags;
    void (*sa_restorer)(void);
} sigaction_t;

MOS_STATIC_ASSERT(sizeof(sigaction_t) == 24, "update sigaction_t struct size");

MOS_STATIC_ASSERT(offsetof(struct sigaction, __sa_handler) == offsetof(sigaction_t, handler), "update sigaction_t struct layout");
MOS_STATIC_ASSERT(offsetof(struct sigaction, sa_flags) == offsetof(sigaction_t, sa_flags), "update sigaction_t struct layout");
MOS_STATIC_ASSERT(offsetof(struct sigaction, sa_restorer) == offsetof(sigaction_t, sa_restorer), "update sigaction_t struct layout");
