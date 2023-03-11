// SPDX-License-Identifier: GPL-3.0-or-later

#include "../console-helpers.h"
#include "libuserspace.h"
#include "mos/syscall/usermode.h"
#include "mos/types.h"

// My mutex implementation:
// ==========================================================================

/**
 * @brief My mutex type.
 */
typedef struct
{
    futex_word_t w;
} my_mutex_t;

/**
 * @brief Initialize the mutex.
 *
 * @param mutex The mutex to initialize.
 */
void my_mutex_init(my_mutex_t *mutex)
{
    MOS_UNUSED(mutex);
}

/**
 * @brief Acquire the mutex.
 *
 * @param mutex The mutex to acquire.
 */
void my_mutex_acquire(my_mutex_t *mutex)
{
    MOS_UNUSED(mutex);
}

/**
 * @brief Release the mutex.
 *
 * @param mutex The mutex to release.
 */
void my_mutex_release(my_mutex_t *mutex)
{
    MOS_UNUSED(mutex);
}

static my_mutex_t my_lock;

// ==========================================================================

const u32 N_THREADS = 20;
const size_t N_WORK = 500000;
static u64 counter = 0;

/**
 * @brief The thread function, which will increment the counter N_WORK times.
 *
 * @param arg The number of times to increment the counter.
 */
static void thread_do_work(void *arg)
{
    print_to_console("-- Thread %ld started!\n", syscall_get_tid());
    my_mutex_acquire(&my_lock);
    for (long i = 0; i < (long) arg; i++)
    {
        u64 current_count = counter;

        // some time consuming work:
        for (u32 i = 0; i < 100; i++)
        {
            volatile u32 j = 0; // prevent compiler from optimizing this loop away
            j += i;
        }

        current_count++;
        counter = current_count;
    }
    my_mutex_release(&my_lock);
    print_to_console("-- Thread %2ld finished!\n", syscall_get_tid());
}

int main(int argc, char **argv)
{
    MOS_UNUSED(argc);
    MOS_UNUSED(argv);
    // setup the console:
    open_console();
    print_to_console("Hello from my mutex test!\n");
    print_to_console("Running...\n");
    set_console_color(LightBlue, Black);

    //
    // Initialize the mutex:
    //
    my_mutex_init(&my_lock);

    //
    // Start N_THREADS threads, each of which will increment the counter N_WORK times.
    //
    tid_t threads[N_THREADS];
    for (u32 i = 0; i < N_THREADS; i++)
        threads[i] = start_thread("my mutex thread", thread_do_work, (void *) N_WORK);
    for (u32 i = 0; i < N_THREADS; i++)
        syscall_wait_for_thread(threads[i]);

    //
    // Check the result:
    //
    const u64 expected = N_WORK * N_THREADS;
    if (counter != expected)
    {
        set_console_color(White, LightRed);
        print_to_console("FAIL: counter value: %llu, where it should be %llu\n", counter, expected);
    }
    else
    {
        set_console_color(White, Green);
        print_to_console("SUCCESS: counter value: %llu\n", counter);
    }

    //
    // Don't exit if our PID is 1 (init process):
    //
    if (syscall_get_pid() == 1)
    {
        while (1)
            ;
    }

    return 0;
}
