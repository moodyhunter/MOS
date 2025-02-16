// SPDX-License-Identifier: GPL-3.0-or-later
// Special processes: (pid 0: idle, pid 1: init, pid 2: kthreadd)

#include "mos/filesystem/sysfs/sysfs.hpp"
#include "mos/filesystem/sysfs/sysfs_autoinit.hpp"
#include "mos/syslog/printk.hpp"
#include "mos/tasks/schedule.hpp"

#include <mos/lib/structures/hashmap.hpp>
#include <mos/lib/structures/hashmap_common.hpp>
#include <mos/misc/panic.hpp>
#include <mos/platform/platform.hpp>
#include <mos/tasks/process.hpp>
#include <mos/tasks/task_types.hpp>
#include <mos/tasks/thread.hpp>
#include <mos_stdlib.hpp>

#define PROCESS_HASHTABLE_SIZE 512
#define THREAD_HASHTABLE_SIZE  512

static void dump_process(void)
{
    if (current_thread)
    {
        Process *proc = current_process;
        pr_info("process %pp ", (void *) proc);
        if (proc->parent)
            pr_info2("parent %pp ", (void *) proc->parent);
        else
            pr_info2("parent <none> ");
        process_dump_mmaps(proc);
    }
    else
    {
        pr_warn("no current thread");
    }
}

MOS_PANIC_HOOK(dump_process, "Dump current process");

// ! sysfs support

static bool tasks_sysfs_process_list(sysfs_file_t *f)
{
    for (const auto &[pid, proc] : ProcessTable)
        sysfs_printf(f, "%pp, parent=%pp, main_thread=%pt, exit_status=%d\n", (void *) proc, (void *) proc->parent, (void *) proc->main_thread, proc->exit_status);

    return true;
}

static bool tasks_sysfs_thread_list(sysfs_file_t *f)
{
    for (const auto &[tid, thread] : thread_table)
        sysfs_printf(f, "%pt, state=%c, mode=%s, owner=%pp, stack=%p (%zu bytes)\n", (void *) thread, thread_state_str(thread->state),
                     thread->mode == THREAD_MODE_KERNEL ? "kernel" : "user", (void *) thread->owner, (void *) thread->u_stack.top, thread->u_stack.capacity);
    return true;
}

static sysfs_item_t task_sysfs_items[] = {
    SYSFS_RO_ITEM("processes", tasks_sysfs_process_list),
    SYSFS_RO_ITEM("threads", tasks_sysfs_thread_list),
};

SYSFS_AUTOREGISTER(tasks, task_sysfs_items);
