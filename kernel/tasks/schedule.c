// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/tasks/schedule.h"

#include "lib/structures/hashmap.h"
#include "mos/platform/platform.h"
#include "mos/printk.h"
#include "mos/tasks/process.h"
#include "mos/tasks/task_type.h"
#include "mos/tasks/thread.h"

void mos_update_current(thread_t *new_current)
{
    thread_t *old_current = current_thread;
    // In the very first switch, current_thread is NULL
    if (likely(old_current))
    {
        // TODO: Add more checks
        if (old_current->status == THREAD_STATUS_RUNNING || current_thread->status != THREAD_STATUS_DEAD)
            old_current->status = THREAD_STATUS_READY;
    }
    current_thread = new_current;
    new_current->status = THREAD_STATUS_RUNNING;
    current_cpu->pagetable = new_current->owner->pagetable;
}

bool schedule_to_thread(const void *key, void *value)
{
    tid_t *tid = (tid_t *) key;
    thread_t *thread = (thread_t *) value;

    MOS_ASSERT_X(thread->tid == *tid, "something is wrong with the thread table");
    if (thread->status == THREAD_STATUS_READY || thread->status == THREAD_STATUS_CREATED)
    {
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
    cpu_t *cpu = current_cpu;
    if (cpu->thread->status != THREAD_STATUS_DEAD)
        cpu->thread->status = THREAD_STATUS_READY;
    platform_switch_to_scheduler(&cpu->thread->stack.head, cpu->scheduler_stack);
}
