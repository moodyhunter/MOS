// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/tasks/process.h"

#include "lib/string.h"
#include "lib/structures/hashmap.h"
#include "mos/io/io.h"
#include "mos/mm/kmalloc.h"
#include "mos/platform/platform.h"
#include "mos/printk.h"
#include "mos/tasks/task_io.h"
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

process_t *allocate_process(process_id_t parent_pid, uid_t euid, thread_entry_t entry, void *arg)
{
    mos_debug("create_process(parent: %d, euid: %d, arg: %p)", parent_pid.process_id, euid.uid, arg);
    process_t *process = kmalloc(sizeof(process_t));
    memzero(process, sizeof(process_t));

    process->magic[0] = 'P';
    process->magic[1] = 'R';
    process->magic[2] = 'O';
    process->magic[3] = 'C';

    process->id = new_process_id();
    process->effective_uid = euid;
    process->parent_pid = parent_pid;

    process->pagetable = mos_platform->mm_create_pagetable();

    process_stdio_setup(process);
    thread_t *main_thread = create_thread(process, THREAD_FLAG_USERMODE, entry, arg);
    process->main_thread_id = main_thread->id;

    void *old_proc = hashmap_put(process_table, &process->id, process);
    MOS_ASSERT_X(old_proc == NULL, "process already exists, go and buy yourself a lottery :)");
    return process;
}

process_t *get_process(process_id_t pid)
{
    process_t *p = hashmap_get(process_table, &pid);
    if (p == NULL)
        mos_warn("process %d not found", pid.process_id);
    return p;
}

fd_t process_add_fd(process_t *process, io_t *file)
{
    MOS_ASSERT(process_is_valid(process));
    int fd = process->files_count++;
    if (fd >= MOS_PROCESS_MAX_OPEN_FILES)
    {
        mos_warn("process %d has too many open files", process->id.process_id);
        return -1;
    }

    process->files[fd] = file;
    return fd;
}

bool process_remove_fd(process_t *process, fd_t fd)
{
    MOS_ASSERT(process_is_valid(process));
    if (fd < 0 || fd >= process->files_count)
        return false;
    io_close(process->files[fd]);
    process->files[fd] = NULL;
    return true;
}
