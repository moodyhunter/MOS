// SPDX-License-Identifier: GPL-3.0-or-later

#include "lib/sync/mutex.h"
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
static mutex_t lock = MUTEX_INIT;
#define LOCK()   mutex_acquire(&lock)
#define UNLOCK() mutex_release(&lock)
#elif LOCK_TYPE == LOCK_NO_LOCK
#define LOCK()
#define UNLOCK()
#else
#error "Invalid lock type!"
#endif

static u64 counter = 0;

static void time_consuming_work(void)
{
    for (u32 i = 0; i < 100; i++)
        ;
}

static void thread_do_work(void *arg)
{
    printf("Thread %d started!\n", syscall_get_tid());
    LOCK();
    for (long i = 0; i < (long) arg; i++)
    {
        u64 current_count = counter;
        time_consuming_work();
        current_count++;
        counter = current_count;
    }
    UNLOCK();
    printf("Thread %d finished!\n", syscall_get_tid());
}

int main(int argc, char **argv)
{
    MOS_UNUSED(argc);
    MOS_UNUSED(argv);
    printf("Threads and Locks Test!\n");

    const u32 N_THREADS = 20;
    const size_t N_WORK = 500000;
    tid_t threads[N_THREADS];

    for (u32 i = 0; i < N_THREADS; i++)
    {
        threads[i] = start_thread("thread", thread_do_work, (void *) N_WORK);
    }

    for (u32 i = 0; i < N_THREADS; i++)
    {
        syscall_wait_for_thread(threads[i]);
    }

    const u64 expected = N_WORK * N_THREADS;

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
