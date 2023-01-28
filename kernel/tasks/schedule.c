// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/tasks/schedule.h"

#include "lib/structures/hashmap.h"
#include "lib/sync/spinlock.h"
#include "mos/platform/platform.h"
#include "mos/printk.h"
#include "mos/tasks/process.h"
#include "mos/tasks/task_types.h"
#include "mos/tasks/thread.h"
#include "mos/tasks/wait.h"

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
            if (!thread->waiting_condition)
                return false;

            if (!wc_condition_verify(thread->waiting_condition))
                return false;
            wc_condition_cleanup(thread->waiting_condition);
            thread->waiting_condition = NULL;
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

static bool schedule_to_thread(const void *key, void *value)
{
    tid_t *tid = (tid_t *) key;
    thread_t *thread = (thread_t *) value;

    MOS_ASSERT_X(thread->tid == *tid, "something is wrong with the thread table");

    spinlock_acquire(&thread->state_lock);
    if (should_schedule_to_thread(thread))
    {
        switch_flags_t switch_flags = 0;
        if (thread->state == THREAD_STATE_CREATED)
            switch_flags |= thread->mode == THREAD_MODE_KERNEL ? SWITCH_TO_NEW_KERNEL_THREAD : SWITCH_TO_NEW_USER_THREAD;

        if (thread->owner->pagetable.pgd != current_cpu->pagetable.pgd)
            switch_flags |= SWITCH_TO_NEW_PAGE_TABLE;

        thread->state = THREAD_STATE_RUNNING;
        spinlock_release(&thread->state_lock);

        current_thread = thread;
        current_cpu->pagetable = thread->owner->pagetable;
        mos_debug(schedule, "cpu %d: switching to thread %d, flags: %c%c%c", //
                  current_cpu->id,                                           //
                  thread->tid,                                               //
                  switch_flags & SWITCH_TO_NEW_PAGE_TABLE ? 'P' : '-',       //
                  switch_flags & SWITCH_TO_NEW_USER_THREAD ? 'U' : '-',      //
                  switch_flags & SWITCH_TO_NEW_KERNEL_THREAD ? 'K' : '-'     //
        );

        platform_switch_to_thread(&current_cpu->scheduler_stack, thread, switch_flags);
    }
    else
    {
        spinlock_release(&thread->state_lock);
    }
    return true;
}

void __cold unblock_scheduler(void)
{
    mos_debug(schedule, "unblocking scheduler");
    MOS_ASSERT_X(!scheduler_ready, "scheduler is already unblocked");
    scheduler_ready = true;
}

noreturn void scheduler(void)
{
    while (likely(!scheduler_ready))
        ; // wait for the scheduler to be unblocked

    mos_debug(schedule, "cpu %d: scheduler is ready", current_cpu->id);

    while (1)
        hashmap_foreach(thread_table, schedule_to_thread);
}

void reschedule_for_wait_condition(wait_condition_t *wait_condition)
{
    thread_t *t = current_cpu->thread;
    MOS_ASSERT_X(t->state != THREAD_STATE_BLOCKED, "thread %d is already blocked", t->tid);
    MOS_ASSERT_X(t->waiting_condition == NULL, "thread %d is already waiting for something else", t->tid);
    spinlock_acquire(&t->state_lock);
    t->state = THREAD_STATE_BLOCKED;
    mos_debug(schedule, "cpu %d: thread %d is now blocked", current_cpu->id, t->tid);
    spinlock_release(&t->state_lock);
    t->waiting_condition = wait_condition;
    reschedule();
}

void reschedule(void)
{
    // A thread can jump to the scheduler if it is:
    // - in RUNNING state       normal condition (context switch caused by timer interrupt or yield())
    // - in CREATED state       the thread is not yet started
    // - in DEAD state          the thread is exiting, and the scheduler will clean it up
    // - in BLOCKED state       the thread is waiting for a condition, and we'll schedule to other threads
    // - in READY state         the thread is the current thread which was blocked, but a suddenly it became ready
    cpu_t *cpu = current_cpu;

    spinlock_acquire(&cpu->thread->state_lock);
    if (cpu->thread->state == THREAD_STATE_RUNNING)
    {
        cpu->thread->state = THREAD_STATE_READY;
        mos_debug(schedule, "cpu %d: rescheduling thread %d, making it ready", cpu->id, cpu->thread->tid);
    }
    spinlock_release(&cpu->thread->state_lock);

    // update k_stack because we are now running on the kernel stack
    platform_switch_to_scheduler(&cpu->thread->k_stack.head, cpu->scheduler_stack);
}
