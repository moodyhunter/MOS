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
    switch (thread->status)
    {
        case THREAD_STATUS_READY:
        case THREAD_STATUS_CREATED:
        {
            return true;
        }
        case THREAD_STATUS_BLOCKED:
        {
            MOS_ASSERT(thread->waiting_condition);
            if (!wc_condition_verify(thread->waiting_condition))
                return false;
            wc_condition_cleanup(thread->waiting_condition);
            thread->waiting_condition = NULL;
            return true;
        }
        case THREAD_STATUS_DEAD:
        {
            return false;
        }
        default:
        {
            mos_panic("Unknown thread status %d", thread->status);
        }
    }
}

void mos_update_current(thread_t *new_current)
{
    thread_t *old_current = current_thread;
    MOS_ASSERT(old_current && new_current);

    // TODO: Add more checks
    if (old_current->status == THREAD_STATUS_RUNNING)
        old_current->status = THREAD_STATUS_READY;

    current_thread = new_current;
    new_current->status = THREAD_STATUS_RUNNING;
    current_cpu->pagetable = new_current->owner->pagetable;
}

bool schedule_to_thread(const void *key, void *value)
{
    tid_t *tid = (tid_t *) key;
    thread_t *thread = (thread_t *) value;

    MOS_ASSERT_X(thread->tid == *tid, "something is wrong with the thread table");
    if (should_schedule_to_thread(thread))
    {
        mos_debug("switching to thread %d", thread->tid);
        platform_switch_to_thread(&current_cpu->scheduler_stack, thread);
    }
    return true;
}

noreturn void scheduler(void)
{
    while (1)
        hashmap_foreach(thread_table, schedule_to_thread);
}

void jump_to_scheduler(void)
{
    // A thread can jump to the scheduler if it is:
    // - in RUNNING state       normal condition (context switch caused by timer interrupt or yield())
    // - in CREATED state       the thread is not yet started
    // - in DEAD state          the thread is exiting, and the scheduler will clean it up
    // - in BLOCKED state       the thread is waiting for a condition, and we'll schedule to other threads
    // but not if it is:
    // - in READY state         the thread should not be running anyway
    cpu_t *cpu = current_cpu;
    MOS_ASSERT(cpu->thread->status != THREAD_STATUS_READY);
    if (cpu->thread->status == THREAD_STATUS_RUNNING)
        cpu->thread->status = THREAD_STATUS_READY;
    platform_switch_to_scheduler(&cpu->thread->stack.head, cpu->scheduler_stack);
}
