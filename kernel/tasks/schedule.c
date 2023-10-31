// SPDX-License-Identifier: GPL-3.0-or-later

#include <mos/lib/structures/hashmap.h>
#include <mos/lib/sync/spinlock.h>
#include <mos/platform/platform.h>
#include <mos/printk.h>
#include <mos/tasks/process.h>
#include <mos/tasks/schedule.h>
#include <mos/tasks/task_types.h>
#include <mos/tasks/thread.h>
#include <mos/tasks/wait.h>

static const char thread_state_str[] = {
    [THREAD_STATE_CREATED] = 'C', //
    [THREAD_STATE_READY] = 'R',   //
    [THREAD_STATE_RUNNING] = 'r', //
    [THREAD_STATE_BLOCKED] = 'B', //
    [THREAD_STATE_DEAD] = 'D',    //
};

static bool scheduler_ready = false;

static bool should_schedule_to_thread(thread_t *thread)
{
    switch (thread->state)
    {
        case THREAD_STATE_READY:
        case THREAD_STATE_CREATED:
        {
            return true;
        }
        case THREAD_STATE_BLOCKED:
        {
            // if the thread is blocked, check if the condition (if any) is met
            if (!thread->waiting)
                return false;

            if (!wc_condition_verify(thread->waiting))
                return false;
            wc_condition_cleanup(thread->waiting);
            thread->waiting = NULL;
            pr_dinfo2(scheduler, "cpu %d: thread %d waiting condition is resolved", current_cpu->id, thread->tid);
            return true;
        }
        case THREAD_STATE_DEAD:
        case THREAD_STATE_RUNNING:
        {
            return false;
        }
        default:
        {
            mos_panic("Unknown thread status %d", thread->state);
        }
    }
}

static bool schedule_to_thread(uintn key, void *value, void *data)
{
    MOS_UNUSED(data);
    tid_t tid = key;
    thread_t *thread = (thread_t *) value;

    MOS_ASSERT_X(thread->tid == tid, "something is wrong with the thread table");

    switch_flags_t switch_flags = 0;
    spinlock_acquire(&thread->state_lock);
    if (!should_schedule_to_thread(thread))
    {
        spinlock_release(&thread->state_lock);
        return true;
    }
    else
    {
        if (thread->state == THREAD_STATE_CREATED)
            switch_flags |= thread->mode == THREAD_MODE_KERNEL ? SWITCH_TO_NEW_KERNEL_THREAD : SWITCH_TO_NEW_USER_THREAD;
        thread->state = THREAD_STATE_RUNNING;
        spinlock_release(&thread->state_lock);
    }

    cpu_t *cpu = current_cpu;
    pr_dinfo2(scheduler, "cpu %d: switching to thread %pt -> %pt, flags: %c%c", //
              cpu->id,                                                          //
              (void *) current_thread,                                          //
              (void *) thread,                                                  //
              switch_flags & SWITCH_TO_NEW_USER_THREAD ? 'U' : '-',             //
              switch_flags & SWITCH_TO_NEW_KERNEL_THREAD ? 'K' : '-'            //
    );

    const bool should_switch_mm = cpu->mm_context != thread->owner->mm;
    if (should_switch_mm)
    {
        mm_context_t *old = mm_switch_context(thread->owner->mm);
        MOS_UNUSED(old);
    }

    cpu->thread = thread;

    platform_switch_to_thread(&cpu->scheduler_stack, thread, switch_flags);
    return true;
}

void __cold unblock_scheduler(void)
{
    pr_dinfo2(scheduler, "unblocking scheduler");
    MOS_ASSERT_X(!scheduler_ready, "scheduler is already unblocked");
    scheduler_ready = true;
}

noreturn void scheduler(void)
{
    while (likely(!scheduler_ready))
        ; // wait for the scheduler to be unblocked

    pr_dinfo2(scheduler, "cpu %d: scheduler is ready", current_cpu->id);

    while (1)
        hashmap_foreach(&thread_table, schedule_to_thread, NULL);
}

void reschedule_for_wait_condition(wait_condition_t *wait_condition)
{
    thread_t *t = current_cpu->thread;
    MOS_ASSERT_X(t->state != THREAD_STATE_BLOCKED, "thread %d is already blocked", t->tid);
    MOS_ASSERT_X(t->waiting == NULL, "thread %d is already waiting for something else", t->tid);
    spinlock_acquire(&t->state_lock);
    t->state = THREAD_STATE_BLOCKED;
    pr_dinfo2(scheduler, "cpu %d: thread %d is now blocked", current_cpu->id, t->tid);
    spinlock_release(&t->state_lock);
    t->waiting = wait_condition;
    platform_switch_to_scheduler(&t->k_stack.head, current_cpu->scheduler_stack);
}

bool reschedule_for_waitlist(waitlist_t *waitlist)
{
    thread_t *t = current_cpu->thread;
    MOS_ASSERT_X(t->state != THREAD_STATE_BLOCKED, "thread %d is already blocked", t->tid);
    MOS_ASSERT_X(t->waiting == NULL, "thread %d is already waiting for something else", t->tid);

    if (!waitlist_append_only(waitlist))
        return false; // waitlist is closed, process is dead

    spinlock_release(&t->state_lock);
    t->state = THREAD_STATE_BLOCKED;
    pr_dinfo2(scheduler, "cpu %d: thread %d is now blocked", current_cpu->id, t->tid);
    spinlock_release(&t->state_lock);
    platform_switch_to_scheduler(&t->k_stack.head, current_cpu->scheduler_stack);

    return true;
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

    spinlock_acquire(&cpu->thread->state_lock);
    MOS_ASSERT_X(cpu->thread->state != THREAD_STATE_READY, "thread %d must not be ready", cpu->thread->tid);

    if (cpu->thread->state == THREAD_STATE_RUNNING)
    {
        cpu->thread->state = THREAD_STATE_READY;
        pr_dinfo2(scheduler, "cpu %d: rescheduling thread %d, making it ready", cpu->id, cpu->thread->tid);
    }
    else
    {
        pr_dinfo2(scheduler, "cpu %d: rescheduling thread %d, state: '%c'", cpu->id, cpu->thread->tid, thread_state_str[cpu->thread->state]);
    }
    spinlock_release(&cpu->thread->state_lock);

    // update k_stack because we are now running on the kernel stack
    platform_switch_to_scheduler(&cpu->thread->k_stack.head, cpu->scheduler_stack);
}
