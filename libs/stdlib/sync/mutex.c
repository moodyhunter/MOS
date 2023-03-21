// SPDX-License-Identifier: GPL-3.0-or-later

#include <mos/lib/sync/mutex.h>

#ifdef __MOS_KERNEL__
#include <mos/locks/futex.h>
#else
#include <mos/syscall/usermode.h>
#define futex_wait(futex, val) syscall_futex_wait(futex, val)
#define futex_wake(futex, val) syscall_futex_wake(futex, val)
#endif

// a mutex_t holds a value of 0 or 1, meaning:
// mutex acquired = 1
// mutex released = 0

void mutex_acquire(mutex_t *m)
{
    while (1)
    {
        mutex_t zero = 0;
        // try setting the mutex to 1 (only if it's 0)
        if (__atomic_compare_exchange_n(m, &zero, 1, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST))
            return;

        // tell the kernel that "the mutex should be 1", and wait until it is 0
        futex_wait(m, 1);
    }
}

void mutex_release(mutex_t *m)
{
    mutex_t one = 1;
    // try setting the mutex to 0 (only if it's 1)
    if (__atomic_compare_exchange_n(m, &one, 0, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST))
    {
        bool result = futex_wake(m, 1); // TODO: Handle error
        MOS_UNUSED(result);
    }
}
