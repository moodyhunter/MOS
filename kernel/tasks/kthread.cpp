// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/tasks/schedule.hpp"

#include <mos/lib/structures/hashmap.hpp>
#include <mos/syslog/printk.hpp>
#include <mos/tasks/kthread.hpp>
#include <mos/tasks/process.hpp>
#include <mos/tasks/task_types.hpp>
#include <mos/tasks/thread.hpp>
#include <mos_stdlib.hpp>

static process_t *kthreadd = NULL;

typedef struct kthread_arg
{
    thread_entry_t entry;
    void *arg;
} kthread_arg_t;

static void kthread_entry(void *arg)
{
    kthread_arg_t *kthread_arg = static_cast<kthread_arg_t *>(arg);
    kthread_arg->entry(kthread_arg->arg);
    kfree(kthread_arg);
    thread_exit(current_thread);
}

void kthread_init(void)
{
    kthreadd = process_allocate(NULL, "kthreadd");
    MOS_ASSERT_X(kthreadd->pid == 2, "kthreadd should have pid 2");
    hashmap_put(&process_table, kthreadd->pid, kthreadd);
}

thread_t *kthread_create(thread_entry_t entry, void *arg, const char *name)
{
    thread_t *thread = kthread_create_no_sched(entry, arg, name);
    scheduler_add_thread(thread);
    return thread;
}

thread_t *kthread_create_no_sched(thread_entry_t entry, void *arg, const char *name)
{
    MOS_ASSERT_X(kthreadd, "kthreadd not initialized");
    pr_dinfo2(thread, "creating kernel thread '%s'", name);
    kthread_arg_t *kthread_arg = (kthread_arg_t *) kmalloc(sizeof(kthread_arg_t));
    kthread_arg->entry = entry;
    kthread_arg->arg = arg;
    auto thread = thread_new(kthreadd, THREAD_MODE_KERNEL, name, 0, NULL);
    if (!thread)
    {
        kfree(kthread_arg);
        pr_fatal("failed to create kernel thread");
        return NULL;
    }

    platform_context_setup_child_thread(thread.get(), kthread_entry, kthread_arg);
    thread_complete_init(thread.get());
    return thread.get();
}
