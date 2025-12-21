// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/platform/platform_defs.hpp"

#include <mos/mos_global.h>
#include <mos/types.hpp>
#include <source_location>

#define barrier() MOS_PLATFORM_MEMORY_BARRIER()

class SpinLocker;
struct spinlock_t
{
    constexpr spinlock_t() : flag(false) {};
    bool flag = false;
    std::source_location locker;
    std::source_location unlocker;

    SpinLocker lock();
};

#define spinlock_init(lock)                                                                                                                                              \
    do                                                                                                                                                                   \
    {                                                                                                                                                                    \
        (lock)->flag = 0;                                                                                                                                                \
    } while (0)

#define _spinlock_real_acquire(lock)                                                                                                                                     \
    do                                                                                                                                                                   \
    {                                                                                                                                                                    \
        barrier();                                                                                                                                                       \
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
        (lock)->locker = std::source_location::current();                                                                                                                \
        (lock)->unlocker = std::source_location();                                                                                                                       \
    } while (0)

#define spinlock_release(lock)                                                                                                                                           \
    do                                                                                                                                                                   \
    {                                                                                                                                                                    \
        (lock)->locker = std::source_location();                                                                                                                         \
        (lock)->unlocker = std::source_location::current();                                                                                                              \
        _spinlock_real_release(lock);                                                                                                                                    \
    } while (0)
#else
#define spinlock_acquire(lock) _spinlock_real_acquire(lock)
#define spinlock_release(lock) _spinlock_real_release(lock)
#endif

#define spinlock_acquire_nodebug(lock) _spinlock_real_acquire(lock)
#define spinlock_release_nodebug(lock) _spinlock_real_release(lock)

should_inline bool spinlock_is_locked(const spinlock_t *lock)
{
    return lock->flag;
}

typedef struct
{
    spinlock_t lock;
    void *owner;
    size_t count;
} recursive_spinlock_t;

// clang-format off
#define RECURSIVE_SPINLOCK_INIT { {}, NULL, 0 }
// clang-format on

should_inline void recursive_spinlock_acquire(recursive_spinlock_t *lock, void *owner)
{
    if (lock->owner == owner)
    {
        lock->count++;
    }
    else
    {
        spinlock_acquire(&lock->lock);
        lock->owner = owner;
        lock->count = 1;
    }
}

should_inline void recursive_spinlock_release(recursive_spinlock_t *lock, void *owner)
{
    if (lock->owner == owner)
    {
        lock->count--;
        if (lock->count == 0)
        {
            lock->owner = NULL;
            spinlock_release(&lock->lock);
        }
    }
}

should_inline bool recursive_spinlock_is_locked(recursive_spinlock_t *lock)
{
    return lock->lock.flag;
}

class [[nodiscard("don't discard")]] SpinUnlocker
{
  public:
    explicit SpinUnlocker(spinlock_t *lock, const std::source_location &loc = std::source_location::current()) : m_lock(lock)
    {
        m_lock->unlocker = loc;
        _spinlock_real_release(m_lock);
    }

    SpinUnlocker(const SpinUnlocker &) = delete;
    SpinUnlocker &operator=(const SpinUnlocker &) = delete;
    SpinUnlocker(SpinUnlocker &&) = delete;
    SpinUnlocker &operator=(SpinUnlocker &&) = delete;

    void discard()
    {
        m_lock = nullptr;
    }

    ~SpinUnlocker()
    {
        if (m_lock)
        {
            m_lock->locker = std::source_location::current();
            _spinlock_real_acquire(m_lock);
        }
    }

  private:
    spinlock_t *m_lock;
};

class [[nodiscard("don't discard")]] SpinLocker
{
  public:
    explicit SpinLocker(spinlock_t *lock, const std::source_location &loc = std::source_location::current()) : m_lock(lock)
    {
        _spinlock_real_acquire(m_lock);
        m_lock->locker = loc;
    }

    SpinLocker(const SpinLocker &) = delete;
    SpinLocker &operator=(const SpinLocker &) = delete;
    SpinLocker(SpinLocker &&) = delete;
    SpinLocker &operator=(SpinLocker &&) = delete;

    void discard()
    {
        m_lock = nullptr;
    }

    ~SpinLocker()
    {
        if (m_lock)
        {
            m_lock->unlocker = std::source_location::current();
            _spinlock_real_release(m_lock);
        }
    }

    SpinUnlocker UnlockTemporarily(const std::source_location &loc = std::source_location::current())
    {
        return SpinUnlocker(m_lock, loc);
    }

  private:
    spinlock_t *m_lock;
};

inline SpinLocker spinlock_t::lock()
{
    return SpinLocker(this);
}
