// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/types.h"

typedef futex_word_t mutex_t;
#define MUTEX_INIT 0

should_inline void mutex_init(mutex_t *mutex)
{
    *mutex = MUTEX_INIT;
}

void mutex_acquire(mutex_t *mutex);
void mutex_release(mutex_t *mutex);
