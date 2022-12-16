// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/tasks/kthread.h"

#include "lib/structures/hashmap.h"
#include "mos/printk.h"
#include "mos/tasks/process.h"
#include "mos/tasks/task_types.h"
#include "mos/tasks/thread.h"

static process_t *kthreadd = NULL;

void kthread_init(void)
{
    kthreadd = process_allocate(NULL, 0, "kthreadd");
    MOS_ASSERT_X(kthreadd->pid == 2, "kthreadd should have pid 2");
    hashmap_put(process_table, &kthreadd->pid, kthreadd);
}

thread_t *kthread_create(thread_entry_t entry, void *arg, const char *name)
{
    thread_t *thread = thread_new(kthreadd, THREAD_MODE_KERNEL, name, entry, arg);
    return thread;
}
