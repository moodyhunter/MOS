// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/tasks/process.h"

#include "lib/string.h"
#include "lib/structures/hashmap.h"
#include "lib/structures/stack.h"
#include "mos/device/block.h"
#include "mos/filesystem/filesystem.h"
#include "mos/io/io.h"
#include "mos/mm/cow.h"
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

process_t *process_allocate(process_t *parent, uid_t euid, const char *name)
{
    process_t *proc = kmalloc(sizeof(process_t));
    memzero(proc, sizeof(process_t));

    proc->magic[0] = 'P';
    proc->magic[1] = 'R';
    proc->magic[2] = 'O';
    proc->magic[3] = 'C';
    proc->name = "<unknown>";

    proc->pid = new_process_id();

    if (likely(parent))
        proc->parent = parent;
    else if (unlikely(proc->pid == 1))
        proc->parent = proc;
    else
    {
        pr_emerg("process %d has no parent", proc->pid);
        kfree(proc);
        return NULL;
    }

    if (name)
    {
        proc->name = kmalloc(strlen(name) + 1);
        strcpy((char *) proc->name, name);
    }

    proc->effective_uid = euid;
    proc->pagetable = platform_mm_create_user_pgd();
    return proc;
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

process_t *process_new(process_t *parent, uid_t euid, const char *name, thread_entry_t entry, void *arg)
{
    process_t *proc = process_allocate(parent, euid, name);
    process_stdio_setup(proc);
    thread_t *main_thread = thread_new(proc, THREAD_FLAG_USERMODE, entry, arg);
    process_attach_thread(proc, main_thread);
    void *old_proc = hashmap_put(process_table, &proc->pid, proc);
    MOS_ASSERT_X(old_proc == NULL, "process already exists, go and buy yourself a lottery :)");
    return proc;
}

process_t *process_get(pid_t pid)
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
    process->files_count--;
    return true;
}

void process_attach_thread(process_t *process, thread_t *thread)
{
    MOS_ASSERT(process_is_valid(process));
    MOS_ASSERT(thread_is_valid(thread));
    MOS_ASSERT(thread->owner == process);
    mos_debug("process %d attached thread %d", process->pid, thread->tid);
    process->threads[process->threads_count++] = thread;
}

void process_attach_mmap(process_t *process, vmblock_t block, vm_type type, bool cow)
{
    MOS_ASSERT(process_is_valid(process));
    process->mmaps = krealloc(process->mmaps, sizeof(proc_vmblock_t) * (process->mmaps_count + 1));
    process->mmaps[process->mmaps_count++] = (proc_vmblock_t){ .vm = block, .type = type, .map_flags = (cow ? MMAP_COW : MMAP_DEFAULT) };
}

void process_handle_exit(process_t *process, int exit_code)
{
    MOS_ASSERT(process_is_valid(process));
    pr_info("process %d exited with code %d", process->pid, exit_code);

    mos_debug("terminating all %lu threads owned by %d", process->threads_count, process->pid);
    for (int i = 0; i < process->threads_count; i++)
    {
        thread_t *thread = process->threads[i];
        if (thread->status == THREAD_STATUS_DEAD)
        {
            mos_warn("thread %d is already dead", thread->tid);
            continue;
        }
        thread->status = THREAD_STATUS_DEAD;
    }
}

void process_dump_mmaps(process_t *process)
{
    for (int i = 0; i < process->mmaps_count; i++)
    {
        proc_vmblock_t block = process->mmaps[i];
        const char *type = "unknown";
        switch (block.type)
        {
            case VMTYPE_APPCODE: type = "code"; break;
            case VMTYPE_APPDATA: type = "data"; break;
            case VMTYPE_STACK: type = "stack"; break;
            case VMTYPE_KSTACK: type = "kernel stack"; break;
            case VMTYPE_FILE: type = "file"; break;
            default: MOS_UNREACHABLE();
        };

        pr_info("block %d: " PTR_FMT ", %zd page(s), %s%sperm: %s%s%s%s%s%s%s -> %s",
                i,                                                 //
                block.vm.vaddr,                                    //
                block.vm.npages,                                   //
                block.map_flags & MMAP_COW ? "cow, " : "",         //
                block.map_flags & MMAP_PRIVATE ? "private, " : "", //
                block.vm.flags & VM_READ ? "r" : "-",              //
                block.vm.flags & VM_WRITE ? "w" : "-",             //
                block.vm.flags & VM_EXEC ? "x" : "-",              //
                block.vm.flags & VM_WRITE_THROUGH ? "W" : "-",     //
                block.vm.flags & VM_CACHE_DISABLED ? "-" : "c",    //
                block.vm.flags & VM_GLOBAL ? "g" : "-",            //
                block.vm.flags & VM_USER ? "u" : "-",              //
                type                                               //
        );
    }
}
