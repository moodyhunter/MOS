// SPDX-License-Identifier: GPL-3.0-or-later
// Special processes: (pid 0: idle, pid 1: init, pid 2: kthreadd)

#include "mos/filesystem/sysfs/sysfs.h"
#include "mos/filesystem/sysfs/sysfs_autoinit.h"
#include "mos/mm/slab_autoinit.h"
#include "mos/printk.h"

#include <mos/lib/structures/hashmap.h>
#include <mos/lib/structures/hashmap_common.h>
#include <mos/panic.h>
#include <mos/platform/platform.h>
#include <mos/tasks/process.h>
#include <mos/tasks/task_types.h>
#include <mos/tasks/thread.h>
#include <mos_stdlib.h>

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

void tasks_init()
{
    hashmap_init(&process_table, PROCESS_HASHTABLE_SIZE, hashmap_identity_hash, hashmap_simple_key_compare);
    hashmap_init(&thread_table, THREAD_HASHTABLE_SIZE, hashmap_identity_hash, hashmap_simple_key_compare);

    panic_hook_declare(dump_process, "Dump current process");
    panic_hook_install(&dump_process_holder);
}

// ! sysfs support

bool _process_do_print(uintn key, void *val, void *data)
{
    MOS_UNUSED(key);
    sysfs_file_t *f = data;
    process_t *proc = val;
    sysfs_printf(f, "%pp\n", (void *) proc);
    return true;
}

bool _thread_do_print(uintn key, void *val, void *data)
{
    MOS_UNUSED(key);
    sysfs_file_t *f = data;
    thread_t *thread = val;
    sysfs_printf(f, "%pt\n", (void *) thread);
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
