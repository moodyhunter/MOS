// SPDX-License-Identifier: GPL-3.0-or-later
// Special processes: (pid 0: idle, pid 1: init, pid 2: kthreadd)

#include "mos/filesystem/sysfs/sysfs.hpp"
#include "mos/filesystem/sysfs/sysfs_autoinit.hpp"
#include "mos/mm/slab_autoinit.hpp"
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

slab_t *process_cache = NULL, *thread_cache = NULL;
SLAB_AUTOINIT("process", process_cache, process_t);
SLAB_AUTOINIT("thread", thread_cache, thread_t);

static void dump_process(void)
{
    if (current_thread)
    {
        process_t *proc = current_process;
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

void tasks_init()
{
    hashmap_init(&process_table, PROCESS_HASHTABLE_SIZE, hashmap_identity_hash, hashmap_simple_key_compare);
    hashmap_init(&thread_table, THREAD_HASHTABLE_SIZE, hashmap_identity_hash, hashmap_simple_key_compare);
}

// ! sysfs support

bool _process_do_print(uintn key, void *val, void *data)
{
    MOS_UNUSED(key);
    sysfs_file_t *f = (sysfs_file_t *) data;
    process_t *proc = (process_t *) val;
    sysfs_printf(f, "%pp, parent=%pp, main_thread=%pt, exit_status=%d\n", (void *) proc, (void *) proc->parent, (void *) proc->main_thread, proc->exit_status);
    return true;
}

bool _thread_do_print(uintn key, void *val, void *data)
{
    MOS_UNUSED(key);
    sysfs_file_t *f = (sysfs_file_t *) data;
    thread_t *thread = (thread_t *) val;
    sysfs_printf(f, "%pt, state=%c, mode=%s, owner=%pp, stack=%p (%zu bytes)\n", (void *) thread, thread_state_str(thread->state),
                 thread->mode == THREAD_MODE_KERNEL ? "kernel" : "user", (void *) thread->owner, (void *) thread->u_stack.top, thread->u_stack.capacity);
    return true;
}

static bool tasks_sysfs_process_list(sysfs_file_t *f)
{
    hashmap_foreach(&process_table, _process_do_print, (void *) f);
    return true;
}

static bool tasks_sysfs_thread_list(sysfs_file_t *f)
{
    hashmap_foreach(&thread_table, _thread_do_print, (void *) f);
    return true;
}

static sysfs_item_t task_sysfs_items[] = {
    SYSFS_RO_ITEM("processes", tasks_sysfs_process_list),
    SYSFS_RO_ITEM("threads", tasks_sysfs_thread_list),
};

SYSFS_AUTOREGISTER(tasks, task_sysfs_items);
