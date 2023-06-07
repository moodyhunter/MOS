// SPDX-License-Identifier: GPL-3.0-or-later
// Special processes: (pid 0: idle, pid 1: init, pid 2: kthreadd)

#include "mos/mm/slab_autoinit.h"

#include <mos/lib/structures/hashmap.h>
#include <mos/lib/structures/hashmap_common.h>
#include <mos/panic.h>
#include <mos/platform/platform.h>
#include <mos/tasks/process.h>
#include <mos/tasks/task_types.h>
#include <mos/tasks/thread.h>
#include <stdlib.h>

#define PROCESS_HASHTABLE_SIZE 512
#define THREAD_HASHTABLE_SIZE  512

slab_t *process_cache = NULL, *thread_cache = NULL;
MOS_SLAB_AUTOINIT("process", process_cache, process_t);
MOS_SLAB_AUTOINIT("thread", thread_cache, thread_t);

static hash_t pid_hash(uintn key)
{
    return (hash_t){ .hash = key };
}

static hash_t tid_hash(uintn key)
{
    return (hash_t){ .hash = key };
}

static void dump_process(void)
{
    if (current_thread)
    {
        process_t *proc = current_process;
        pr_info("process %ld (%s) ", proc->pid, proc->name);
        if (proc->parent)
            pr_info2("parent %ld (%s) ", proc->parent->pid, proc->parent->name);
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
    pr_info("initializing task management subsystem");
    hashmap_init(&process_table, PROCESS_HASHTABLE_SIZE, pid_hash, hashmap_simple_key_compare);
    hashmap_init(&thread_table, THREAD_HASHTABLE_SIZE, tid_hash, hashmap_simple_key_compare);

    declare_panic_hook(dump_process, "Dump current process");
    install_panic_hook(&dump_process_holder);
}
