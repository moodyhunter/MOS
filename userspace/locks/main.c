// SPDX-License-Identifier: GPL-3.0-or-later

#include "lib/sync/spinlock.h"
#include "libuserspace.h"
#include "mos/syscall/usermode.h"
#include "mos/types.h"

static spinlock_t lock = SPINLOCK_INIT;
static u64 counter = 0;

#define USE_LOCKS 1

#if USE_LOCKS
#define LOCK()   spinlock_acquire(&lock)
#define UNLOCK() spinlock_release(&lock)
#else
#define LOCK()
#define UNLOCK()
#endif

static void thread_do_work(void *arg)
{
    printf("Thread %d started!\n", syscall_get_tid());
    for (u32 i = 0; i < (u32) arg; i++)
    {
        LOCK();
        counter++;
        UNLOCK();
    }
}

int main()
{
    printf("Threads and Locks Test!\n");

    tid_t t1 = start_thread("thread1", thread_do_work, (void *) 1000000);
    tid_t t2 = start_thread("thread2", thread_do_work, (void *) 1000000);
    tid_t t3 = start_thread("thread3", thread_do_work, (void *) 1000000);
    tid_t t4 = start_thread("thread4", thread_do_work, (void *) 1000000);
    tid_t t5 = start_thread("thread5", thread_do_work, (void *) 1000000);

    syscall_wait_for_thread(t1);
    syscall_wait_for_thread(t2);
    syscall_wait_for_thread(t3);
    syscall_wait_for_thread(t4);
    syscall_wait_for_thread(t5);

    printf("Counter: %d\n", counter);

    return 0;
}
