// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/types.h"

enum
{
    MUTEX_LOCKED = true,
    MUTEX_UNLOCKED = false,
};

void mutex_try_acquire_may_reschedule(bool *lock);
bool mutex_release(bool *lock);
