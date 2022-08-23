// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/tasks/process.h"

#include "lib/string.h"
#include "lib/structures/hashmap.h"
#include "mos/mm/kmalloc.h"
#include "mos/printk.h"
#include "mos/tasks/task_type.h"
#include "mos/tasks/thread.h"
#include "mos/types.h"

#define PROCESS_HASHTABLE_SIZE 512

hashmap_t *process_table;

static hash_t process_hash(const void *key)
{
    return (hash_t){ .hash = ((process_id_t *) key)->process_id };
}

static int process_equal(const void *key1, const void *key2)
{
    return ((process_id_t *) key1)->process_id == ((process_id_t *) key2)->process_id;
}

static process_id_t new_process_id(void)
{
    static process_id_t next = { 1 };
    return (process_id_t){ next.process_id++ };
}

void process_init(void)
{
    process_table = kmalloc(sizeof(hashmap_t));
    memset(process_table, 0, sizeof(hashmap_t));
    hashmap_init(process_table, PROCESS_HASHTABLE_SIZE, process_hash, process_equal);
}

void process_deinit(void)
{
    hashmap_deinit(process_table);
    kfree(process_table);
}

process_id_t create_process(process_id_t parent_pid, uid_t euid, thread_entry_t entry, void *arg)
{
    mos_debug("create_process(parent: %d, euid: %d, arg: %p)", parent_pid.process_id, euid.uid, arg);
    process_t *process = kmalloc(sizeof(process_t));
    memset(process, 0, sizeof(process_t));

    process->pid = new_process_id();
    process->effective_uid = euid;
    process->parent_pid = parent_pid;

    // TODO: create page directory
    process->page_dir = NULL;

    // TODO: allocate memory for the process
    process->main_thread_id = create_thread(process->pid, THREAD_FLAG_USERMODE, entry, arg);

    void *old_proc = hashmap_put(process_table, &process->pid, process);
    MOS_ASSERT_X(old_proc == NULL, "process already exists, go and buy yourself a lottery :)");
    return process->pid;
}

process_t *get_process(process_id_t pid)
{
    process_t *p = hashmap_get(process_table, &pid);
    if (p == NULL)
        mos_warn("process %d not found", pid.process_id);
    return p;
}
