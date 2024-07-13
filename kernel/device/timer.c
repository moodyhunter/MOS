// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/device/timer.h"

#include "mos/device/clocksource.h"
#include "mos/lib/structures/list.h"
#include "mos/lib/sync/spinlock.h"
#include "mos/tasks/schedule.h"
#include "mos/tasks/signal.h"

static list_head timer_queue = LIST_HEAD_INIT(timer_queue); ///< list of timers that are waiting to be executed
static spinlock_t timer_queue_lock = SPINLOCK_INIT;

static bool timer_do_wakeup(ktimer_t *timer, void *arg)
{
    MOS_UNUSED(arg);

    if (timer->thread != NULL)
        scheduler_wake_thread(timer->thread);

    return true;
}

void timer_tick()
{
    spinlock_acquire(&timer_queue_lock);
    list_foreach(ktimer_t, timer, timer_queue)
    {
        if (active_clocksource_ticks() >= (u64) timer->timeout)
        {
            if (timer->callback(timer, timer->arg))
                timer->ticked = true, list_remove(timer);
        }
    }
    spinlock_release(&timer_queue_lock);
}

long timer_msleep(u64 ms)
{
    if (!active_clocksource)
        return 0;
    const u64 offset = ms * active_clocksource->frequency / 1000;
    const u64 target_val = active_clocksource_ticks() + offset;

    ktimer_t timer = {
        .list_node = LIST_NODE_INIT(timer),
        .timeout = target_val,
        .thread = current_thread,
        .ticked = false,
        .callback = timer_do_wakeup,
        .arg = NULL,
    };

    spinlock_acquire(&timer_queue_lock);
    list_node_append(&timer_queue, list_node(&timer));
    spinlock_release(&timer_queue_lock);

    while (!timer.ticked)
    {
        blocked_reschedule();
        if (signal_has_pending())
        {
            spinlock_acquire(&timer_queue_lock);
            list_remove(&timer);
            spinlock_release(&timer_queue_lock);
            return -EINTR; // interrupted by signal
        }
    }

    MOS_ASSERT(list_is_empty(list_node(&timer)));
    return 0;
}
