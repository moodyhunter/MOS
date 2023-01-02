// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/tasks/schedule.h"

#include "lib/structures/hashmap.h"
#include "mos/platform/platform.h"
#include "mos/printk.h"
#include "mos/tasks/process.h"
#include "mos/tasks/task_types.h"
#include "mos/tasks/thread.h"
#include "mos/tasks/wait.h"

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
            MOS_ASSERT(thread->waiting_condition);
            if (!wc_condition_verify(thread->waiting_condition))
                return false;
            wc_condition_cleanup(thread->waiting_condition);
            thread->waiting_condition = NULL;
            return true;
        }
        case THREAD_STATE_DEAD:
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
    if (should_schedule_to_thread(thread))
    {
        mos_debug(schedule, "switching to thread %d", thread->tid);
        platform_switch_to_thread(&current_cpu->scheduler_stack, thread);
    }
    return true;
}

void mos_update_current(thread_t *current)
{
    cpu_t *cpu = current_cpu;
    thread_t *previous = cpu->thread;
    MOS_ASSERT(previous && current);

    if (previous->state == THREAD_STATE_RUNNING)
        previous->state = THREAD_STATE_READY;

    current->state = THREAD_STATE_RUNNING;
    cpu->thread = current;
    cpu->pagetable = current->owner->pagetable;
}

noreturn void scheduler(void)
{
    while (1)
        hashmap_foreach(thread_table, schedule_to_thread);
}

void reschedule_for_wait_condition(wait_condition_t *wait_condition)
{
    thread_t *t = current_cpu->thread;
    MOS_ASSERT_X(t->state != THREAD_STATE_BLOCKED, "thread %d is already blocked", t->tid);
    MOS_ASSERT_X(t->waiting_condition == NULL, "thread %d is already waiting for something else", t->tid);
    t->state = THREAD_STATE_BLOCKED;
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
    // but not if it is:
    // - in READY state         the thread should not be running anyway
    cpu_t *cpu = current_cpu;
    MOS_ASSERT(cpu->thread->state != THREAD_STATE_READY);
    if (cpu->thread->state == THREAD_STATE_RUNNING)
        cpu->thread->state = THREAD_STATE_READY;

    // update k_stack because we are now running on the kernel stack
    platform_switch_to_scheduler(&cpu->thread->k_stack.head, cpu->scheduler_stack);
}
