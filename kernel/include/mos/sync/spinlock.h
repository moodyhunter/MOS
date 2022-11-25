// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/mos_global.h"
#include "mos/types.h"

typedef struct
{
    bool flag;
} spinlock_t;

// clang-format off
#define SPINLOCK_INIT { ATOMIC_FLAG_INIT }
// clang-format on

should_inline void spinlock_acquire(spinlock_t *lock)
{
    while (__atomic_test_and_set(&lock->flag, __ATOMIC_ACQUIRE))
        ;
}

should_inline void spinlock_release(spinlock_t *lock)
{
    __atomic_clear(&lock->flag, __ATOMIC_RELEASE);
}
