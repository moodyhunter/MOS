// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/tasks/schedule.hpp"

#include <mos/lib/structures/hashmap.hpp>
#include <mos/syslog/printk.hpp>
#include <mos/tasks/kthread.hpp>
#include <mos/tasks/process.hpp>
#include <mos/tasks/task_types.hpp>
#include <mos/tasks/thread.hpp>
#include <mos/type_utils.hpp>
#include <mos_stdlib.hpp>

static Process *kthreadd = NULL;

struct kthread_arg_t : mos::NamedType<"KThread.Arg">
{
    thread_entry_t entry;
    void *arg;
};

static void kthread_entry(void *arg)
{
    kthread_arg_t *kthread_arg = static_cast<kthread_arg_t *>(arg);
    kthread_arg->entry(kthread_arg->arg);
    delete kthread_arg;
    thread_exit(current_thread);
}

void kthread_init(void)
{
    kthreadd = Process::New(NULL, "kthreadd");
    MOS_ASSERT_X(kthreadd->pid == 2, "kthreadd should have pid 2");
    ProcessTable.insert(kthreadd->pid, kthreadd);
}

Thread *kthread_create(thread_entry_t entry, void *arg, const char *name)
{
    Thread *thread = kthread_create_no_sched(entry, arg, name);
    scheduler_add_thread(thread);
    return thread;
}

Thread *kthread_create_no_sched(thread_entry_t entry, void *arg, const char *name)
{
    MOS_ASSERT_X(kthreadd, "kthreadd not initialized");
    pr_dinfo2(thread, "creating kernel thread '%s'", name);
    kthread_arg_t *kthread_arg = mos::create<kthread_arg_t>();
    kthread_arg->entry = entry;
    kthread_arg->arg = arg;
    auto thread = thread_new(kthreadd, THREAD_MODE_KERNEL, name, 0, NULL);
    if (!thread)
    {
        delete kthread_arg;
        pr_fatal("failed to create kernel thread");
        return NULL;
    }

    platform_context_setup_child_thread(thread.get(), kthread_entry, kthread_arg);
    thread_complete_init(thread.get());
    return thread.get();
}
