// SPDX-License-Identifier: GPL-3.0-or-later

#include "lib/sync/spinlock.h"
#include "libuserspace.h"
#include "mos/syscall/usermode.h"
#include "mos/types.h"

#define LOCK_SPINLOCK     1
#define LOCK_KERNEL_MUTEX 2
#define LOCK_NO_LOCK      3

#define LOCK_TYPE LOCK_KERNEL_MUTEX

#if LOCK_TYPE == LOCK_SPINLOCK
static spinlock_t lock = SPINLOCK_INIT;
#define LOCK()   spinlock_acquire(&lock)
#define UNLOCK() spinlock_release(&lock)
#elif LOCK_TYPE == LOCK_KERNEL_MUTEX
typedef bool my_mutex_t;
static my_mutex_t lock = false;
#define LOCK()   syscall_mutex_acquire(&lock)
#define UNLOCK() syscall_mutex_release(&lock)
#elif LOCK_TYPE == LOCK_NO_LOCK
#define LOCK()
#define UNLOCK()
#else
#error "Invalid lock type!"
#endif

static u64 counter = 0;

static void time_consuming_work()
{
    for (u32 i = 0; i < 100; i++)
        ;
}

static void thread_do_work(void *arg)
{
    printf("Thread %d started!\n", syscall_get_tid());
    LOCK();
    for (u32 i = 0; i < (u32) arg; i++)
    {
        u64 current_count = counter;
        time_consuming_work();
        current_count++;
        counter = current_count;
    }
    UNLOCK();
    printf("Thread %d finished!\n", syscall_get_tid());
}

int main()
{
    printf("Threads and Locks Test!\n");

    tid_t t1 = start_thread("thread1", thread_do_work, (void *) 1000000);
    tid_t t2 = start_thread("thread2", thread_do_work, (void *) 1000000);
    tid_t t3 = start_thread("thread3", thread_do_work, (void *) 1000000);

    syscall_wait_for_thread(t1);
    syscall_wait_for_thread(t2);
    syscall_wait_for_thread(t3);

    const u64 expected = 1000000 * 3;

    if (counter != expected)
    {
        printf("FAIL: counter value: %llu, where it should be %llu\n", counter, expected);
    }
    else
    {
        printf("SUCCESS: counter value: %llu\n", counter);
    }

    return 0;
}
