// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/mos_global.h"
#include "mos/types.h"

#include <stdatomic.h>

typedef struct
{
    atomic_flag flag;
} spinlock_t;

// clang-format off
#define SPINLOCK_INIT { ATOMIC_FLAG_INIT }
// clang-format on

should_inline void spinlock_acquire(spinlock_t *lock)
{
    while (atomic_flag_test_and_set_explicit(&lock->flag, memory_order_acquire))
        ;
}

should_inline void spinlock_release(spinlock_t *lock)
{
    atomic_flag_clear_explicit(&lock->flag, memory_order_release);
}
