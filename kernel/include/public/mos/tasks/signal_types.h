// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

typedef enum
{
    // catchable signals
    SIGNAL_TERM,
    SIGNAL_INTERRUPT,

    _SIGNAL_CATCHABLE_MAX,

    // uncatchable signals
    SIGNAL_KILL,
} signal_t;

typedef void (*signal_action_t)(signal_t signal);

#define SIGNAL_DEFAULT ((signal_action_t) 0)
#define SIGNAL_IGNORE  ((signal_action_t) 1)
