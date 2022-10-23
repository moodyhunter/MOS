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
    return (hash_t){ .hash = *(pid_t *) key };
}

static int process_equal(const void *key1, const void *key2)
{
    return *(pid_t *) key1 == *(pid_t *) key2;
}

static pid_t new_process_id(void)
{
    static pid_t next = 1;
    return (pid_t){ next++ };
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

process_t *allocate_process(process_t *parent, uid_t euid, const char *name, thread_entry_t entry, void *arg)
{
    process_t *proc = kmalloc(sizeof(process_t));
    memzero(proc, sizeof(process_t));

    proc->magic[0] = 'P';
    proc->magic[1] = 'R';
    proc->magic[2] = 'O';
    proc->magic[3] = 'C';

    proc->pid = new_process_id();

    if (proc->pid == 1)
    {
        proc->parent_pid = proc->pid;
    }
    else
    {
        if (!parent)
        {
            pr_emerg("process %d has no parent", proc->pid);
            kfree(proc);
            return NULL;
        }
        proc->parent_pid = parent->pid;
    }

    proc->name = "<unknown>";
    if (name)
    {
        proc->name = kmalloc(strlen(name) + 1);
        strcpy((char *) proc->name, name);
    }

    proc->effective_uid = euid;
    proc->pagetable = mos_platform->mm_create_pagetable();

    process_stdio_setup(proc);
    thread_t *main_thread = create_thread(proc, THREAD_FLAG_USERMODE, entry, arg);
    proc->main_thread_id = main_thread->tid;

    void *old_proc = hashmap_put(process_table, &proc->pid, proc);
    MOS_ASSERT_X(old_proc == NULL, "process already exists, go and buy yourself a lottery :)");
    return proc;
}

process_t *get_process(pid_t pid)
{
    process_t *p = hashmap_get(process_table, &pid);
    if (p == NULL)
        mos_warn("process %d not found", pid);
    return p;
}

fd_t process_attach_fd(process_t *process, io_t *file)
{
    MOS_ASSERT(process_is_valid(process));
    int fd = process->files_count++;
    if (fd >= MOS_PROCESS_MAX_OPEN_FILES)
    {
        mos_warn("process %d has too many open files", process->pid);
        return -1;
    }

    process->files[fd] = file;
    return fd;
}

bool process_detach_fd(process_t *process, fd_t fd)
{
    MOS_ASSERT(process_is_valid(process));
    if (fd < 0 || fd >= process->files_count)
        return false;
    io_close(process->files[fd]);
    process->files[fd] = NULL;
    return true;
}

void process_handle_exit(process_t *process, int exit_code)
{
    MOS_ASSERT(process_is_valid(process));
    pr_info("process %d exited with code %d", process->pid, exit_code);

    // TODO
    // for all threads in process
    //   kill thread

    current_thread->status = THREAD_STATUS_DEAD;
}

process_t *process_handle_fork(process_t *process)
{
    MOS_ASSERT(process_is_valid(process));
    pr_info("process %d forked", process->pid);

    // TODO
    return NULL;
}
