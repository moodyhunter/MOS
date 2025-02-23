// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/tasks/schedule.hpp"

#include "mos/assert.hpp"
#include "mos/lib/sync/spinlock.hpp"
#include "mos/misc/setup.hpp"
#include "mos/platform/platform.hpp"
#include "mos/tasks/scheduler.hpp"
#include "mos/tasks/thread.hpp"

#include <mos_string.hpp>

char thread_state_str(thread_state_t state)
{
    switch (state)
    {
        case THREAD_STATE_CREATED: return 'C';
        case THREAD_STATE_READY: return 'R';
        case THREAD_STATE_RUNNING: return 'r';
        case THREAD_STATE_BLOCKED: return 'B';
        case THREAD_STATE_NONINTERRUPTIBLE: return 'N';
        case THREAD_STATE_DEAD: return 'D';
    }

    MOS_UNREACHABLE();
}

static bool scheduler_ready = false;
static scheduler_t *active_scheduler = NULL;
extern const scheduler_info_t __MOS_SCHEDULERS_START[], __MOS_SCHEDULERS_END[];

MOS_SETUP("scheduler", scheduler_cmdline_selector)
{
    for (const scheduler_info_t *info = __MOS_SCHEDULERS_START; info < __MOS_SCHEDULERS_END; info++)
    {
        if (info->name == arg)
        {
            active_scheduler = info->scheduler;
            active_scheduler->ops->init(active_scheduler);
            pr_dinfo2(scheduler, "active scheduler: %s", info->name);
            return true;
        }
    }

    pr_dwarn(scheduler, "scheduler '%s' not found", arg);
    return false;
}

void scheduler_init()
{
    if (!active_scheduler)
    {
        pr_dwarn(scheduler, "no scheduler is selected, using the first scheduler");
        active_scheduler = __MOS_SCHEDULERS_START[0].scheduler;
        active_scheduler->ops->init(active_scheduler);
    }
}

void unblock_scheduler(void)
{
    pr_dinfo2(scheduler, "unblocking scheduler");
    MOS_ASSERT_X(!scheduler_ready, "scheduler is already unblocked");
    scheduler_ready = true;
}

[[noreturn]] void enter_scheduler(void)
{
    while (likely(!scheduler_ready))
        ; // wait for the scheduler to be unblocked

    pr_dinfo2(scheduler, "cpu %d: scheduler is ready", platform_current_cpu_id());
    MOS_ASSERT(current_thread == nullptr);
    reschedule();
    MOS_UNREACHABLE();
}

void scheduler_add_thread(Thread *thread)
{
    MOS_ASSERT(thread_is_valid(thread));
    MOS_ASSERT_X(thread->state == THREAD_STATE_CREATED || thread->state == THREAD_STATE_READY, "thread %pt is not in a valid state", thread);
    active_scheduler->ops->add_thread(active_scheduler, thread);
}

void scheduler_remove_thread(Thread *thread)
{
    MOS_ASSERT(thread_is_valid(thread));
    active_scheduler->ops->remove_thread(active_scheduler, thread);
}

void scheduler_wake_thread(Thread *thread)
{
    spinlock_acquire(&thread->state_lock);
    if (thread->state == THREAD_STATE_READY || thread->state == THREAD_STATE_RUNNING || thread->state == THREAD_STATE_CREATED || thread->state == THREAD_STATE_DEAD)
    {
        spinlock_release(&thread->state_lock);
        return; // thread is already running or ready
    }

    MOS_ASSERT_X(thread->state == THREAD_STATE_BLOCKED || thread->state == THREAD_STATE_NONINTERRUPTIBLE, "thread %pt is not blocked", thread);
    thread->state = THREAD_STATE_READY;
    spinlock_release(&thread->state_lock);
    pr_dinfo2(scheduler, "waking up %pt", thread);
    active_scheduler->ops->add_thread(active_scheduler, thread);
}

void reschedule(void)
{
    // A thread can jump to the scheduler if it is:
    // - in RUNNING state       normal condition (context switch caused by timer interrupt or yield())
    // - in CREATED state       the thread is not yet started
    // - in DEAD state          the thread is exiting, and the scheduler will clean it up
    // - in BLOCKED state       the thread is waiting for a condition, and we'll schedule to other threads
    // But it can't be:
    // - in READY state
    cpu_t *cpu = current_cpu;

    auto next = active_scheduler->ops->select_next(active_scheduler);

    if (!next)
    {
        if (current_thread && current_thread->state == THREAD_STATE_RUNNING)
        {
            // give the current thread another chance to run, if it's the only one and it's able to run
            MOS_ASSERT_X(spinlock_is_locked(&current_thread->state_lock), "thread state lock must be held");
            pr_dinfo2(scheduler, "no thread to run, staying with %pt, state = %c", current_thread, thread_state_str(current_thread->state));
            spinlock_release(&current_thread->state_lock);
            return;
        }

        next = cpu->idle_thread;
    }

    const bool should_switch_mm = cpu->mm_context != next->owner->mm;
    if (should_switch_mm)
    {
        MMContext *old = mm_switch_context(next->owner->mm);
        MOS_UNUSED(old);
    }

    const switch_flags_t switch_flags = statement_expr(switch_flags_t, {
        retval = SWITCH_REGULAR;
        if (next->state == THREAD_STATE_CREATED)
            retval |= next->mode == THREAD_MODE_KERNEL ? SWITCH_TO_NEW_KERNEL_THREAD : SWITCH_TO_NEW_USER_THREAD;
    });

    if (likely(current_thread))
    {
        if (current_thread->state == THREAD_STATE_RUNNING)
        {
            current_thread->state = THREAD_STATE_READY;
            if (current_thread != cpu->idle_thread)
                scheduler_add_thread(current_thread);
        }
        pr_dinfo2(scheduler, "leaving %pt, state: '%c'", current_thread, thread_state_str(current_thread->state));
    }
    pr_dinfo2(scheduler, "switching to %pt, state: '%c'", next, thread_state_str(next->state));

    next->state = THREAD_STATE_RUNNING;
    spinlock_release(&next->state_lock);
    platform_switch_to_thread(current_thread, next, switch_flags);
}

void blocked_reschedule(void)
{
    spinlock_acquire(&current_thread->state_lock);
    current_thread->state = THREAD_STATE_BLOCKED;
    pr_dinfo2(scheduler, "%pt is now blocked", current_thread);
    reschedule();
}

bool reschedule_for_waitlist(waitlist_t *waitlist)
{
    MOS_ASSERT_X(current_thread->state != THREAD_STATE_BLOCKED, "thread %d is already blocked", current_thread->tid);

    if (!waitlist_append(waitlist))
        return false; // waitlist is closed, process is dead

    blocked_reschedule();
    return true;
}
