// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/tasks/schedule.h"

#include "lib/structures/hashmap.h"
#include "mos/platform/platform.h"
#include "mos/printk.h"
#include "mos/tasks/task_type.h"
#include "mos/tasks/thread.h"

bool schedule_to_thread(const void *key, void *value)
{
    tid_t *tid = (tid_t *) key;
    thread_t *thread = (thread_t *) value;

    MOS_ASSERT_X(thread->tid == *tid, "something is wrong with the thread table");
    if (thread->status == THREAD_STATUS_READY)
    {
        current_thread = thread;
        mos_platform->switch_to_thread(&current_cpu->scheduler_stack, thread);
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
    mos_platform->switch_to_scheduler(&current_thread->stack.head, current_cpu->scheduler_stack);
}
