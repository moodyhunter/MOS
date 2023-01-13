// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/mos_global.h"
#include "mos/types.h"

typedef struct
{
    bool flag;
#if MOS_DEBUG_FEATURE(spinlock)
    const char *file;
    int line;
#endif
} spinlock_t;

// clang-format off
#define SPINLOCK_INIT { 0 }
// clang-format on

#define _spinlock_real_acquire(lock)                                                                                                                                     \
    do                                                                                                                                                                   \
    {                                                                                                                                                                    \
        while (__atomic_test_and_set(&(lock)->flag, __ATOMIC_ACQUIRE))                                                                                                   \
            ;                                                                                                                                                            \
    } while (0)

#define _spinlock_real_release(lock)                                                                                                                                     \
    do                                                                                                                                                                   \
    {                                                                                                                                                                    \
        __atomic_clear(&(lock)->flag, __ATOMIC_RELEASE);                                                                                                                 \
    } while (0)

#if MOS_DEBUG_FEATURE(spinlock)
#define spinlock_acquire(lock)                                                                                                                                           \
    do                                                                                                                                                                   \
    {                                                                                                                                                                    \
        _spinlock_real_acquire(lock);                                                                                                                                    \
        (lock)->file = __FILE__;                                                                                                                                         \
        (lock)->line = __LINE__;                                                                                                                                         \
    } while (0)
#define spinlock_release(lock)                                                                                                                                           \
    do                                                                                                                                                                   \
    {                                                                                                                                                                    \
        (lock)->file = NULL;                                                                                                                                             \
        (lock)->line = 0;                                                                                                                                                \
        _spinlock_real_release(lock);                                                                                                                                    \
    } while (0)
#else
#define spinlock_acquire(lock) _spinlock_real_acquire(lock)
#define spinlock_release(lock) _spinlock_real_release(lock)
#endif

#define spinlock_acquire_nodebug(lock) _spinlock_real_acquire(lock)
#define spinlock_release_nodebug(lock) _spinlock_real_release(lock)

should_inline bool spinlock_is_locked(spinlock_t *lock)
{
    return lock->flag;
}
