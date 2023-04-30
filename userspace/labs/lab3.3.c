// SPDX-License-Identifier: GPL-3.0-or-later

#include <mos/lib/sync/mutex.h>
#include <mos/x86/delays.h>
#include <stdio.h>
#include <stdlib.h>

static mutex_t my_lock = MUTEX_INIT;

void acquire_mutex_but_dont_release(void *arg)
{
    MOS_UNUSED(arg);
    mutex_acquire(&my_lock);

    puts("Bad Thread: I have the lock!");
    puts("Bad Thread: I'm going to die now!");
}

int main()
{
    // start 1 thread
    start_thread("Bad Thread", &acquire_mutex_but_dont_release, NULL);

    // sleep for a while
    puts("Main thread: sleeping for 5 seconds...");
    mdelay(5000);

    // try to acquire the lock
    puts("Main thread: trying to acquire the lock...");
    mutex_acquire(&my_lock);
    puts("Main thread: acquired the lock!");

    // release the lock
    puts("Main thread: releasing the lock...");
    mutex_release(&my_lock);

    return 0;
}
