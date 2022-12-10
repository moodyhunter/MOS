// SPDX-License-Identifier: GPL-3.0-or-later

#include "include/mos/tasks/thread.h"
#include "mos/elf/elf.h"
#include "mos/filesystem/filesystem.h"
#include "mos/mos_global.h"
#include "mos/platform/platform.h"
#include "mos/printk.h"
#include "mos/syscall/decl.h"
#include "mos/tasks/process.h"
#include "mos/tasks/schedule.h"
#include "mos/tasks/task_type.h"
#include "mos/tasks/wait.h"
#include "mos/types.h"

void define_syscall(panic)(void)
{
    if (current_process->effective_uid == 0)
        mos_panic("Kernel panic called by syscall from process %d (%s), thread %d", current_process->pid, current_process->name, current_thread->tid);
    else
        mos_warn("only root can panic");
}

fd_t define_syscall(file_open)(const char *path, file_open_flags flags)
{
    if (path == NULL)
        return -1;

    file_t *f = vfs_open(path, flags);
    if (!f)
        return -1;
    return process_attach_ref_fd(current_process, &f->io);
}

bool define_syscall(file_stat)(const char *path, file_stat_t *stat)
{
    if (path == NULL || stat == NULL)
        return false;

    return vfs_stat(path, stat);
}

size_t define_syscall(io_read)(fd_t fd, void *buf, size_t count, size_t offset)
{
    if (fd < 0 || buf == NULL)
        return 0;
    if (offset)
        mos_warn("offset is not supported yet");
    return io_read(current_process->files[fd], buf, count);
}

size_t define_syscall(io_write)(fd_t fd, const void *buf, size_t count, size_t offset)
{
    if (fd < 0 || buf == NULL)
        return 0;
    if (offset)
        mos_warn("offset is not supported yet");
    return io_write(current_process->files[fd], buf, count);
}

bool define_syscall(io_close)(fd_t fd)
{
    if (fd < 0)
        return false;
    process_detach_fd(current_process, fd);
    return true;
}

noreturn void define_syscall(exit)(u32 exit_code)
{
    int pid = current_process->pid;
    if (unlikely(pid == 1))
        mos_panic("init process exited with code %d", exit_code);

    pr_info2("Kernel syscall exit called with code %d from pid %d", exit_code, pid);
    process_handle_exit(current_process, exit_code);
    jump_to_scheduler();
    MOS_UNREACHABLE();
}

void define_syscall(yield_cpu)(void)
{
    jump_to_scheduler();
}

pid_t define_syscall(fork)(void)
{
    process_t *parent = current_process;
    process_t *child = process_handle_fork(parent);
    if (child == NULL)
        return 0;

    return current_process == child ? 0 : child->pid; // return 0 for child, pid for parent
}

pid_t define_syscall(exec)(const char *path, const char *const argv[])
{
    MOS_UNUSED(path);
    MOS_UNUSED(argv);
    mos_warn("exec syscall not implemented yet");
    return -1;
}

pid_t define_syscall(get_pid)()
{
    MOS_ASSERT(current_process);
    return current_process->pid;
}

pid_t define_syscall(get_parent_pid)()
{
    MOS_ASSERT(current_process && current_process->parent);
    return current_process->parent->pid;
}

pid_t define_syscall(spawn)(const char *path, int argc, const char *const argv[])
{
    MOS_UNUSED(argc);
    MOS_UNUSED(argv);
    process_t *current = current_process;
    process_t *process = elf_create_process(path, current, current->terminal, current->effective_uid);
    if (process == NULL)
        return -1;

    return process->pid;
}

tid_t define_syscall(create_thread)(const char *name, thread_entry_t entry, void *arg)
{
    MOS_UNUSED(name);
    MOS_ASSERT(current_thread);
    thread_t *thread = thread_new(current_process, THREAD_FLAG_USERMODE, entry, arg);
    if (thread == NULL)
        return -1;

    return thread->tid;
}

tid_t define_syscall(get_tid)()
{
    MOS_ASSERT(current_thread);
    return current_thread->tid;
}

noreturn void define_syscall(thread_exit)(void)
{
    MOS_ASSERT(current_thread);
    pr_info2("Kernel syscall thread_exit called from tid %d", current_thread->tid);
    thread_handle_exit(current_thread);
    jump_to_scheduler();
    MOS_UNREACHABLE();
}

uintptr_t define_syscall(heap_control)(heap_control_op op, uintptr_t arg)
{
    MOS_ASSERT(current_process);
    process_t *process = current_process;

    proc_vmblock_t *block = NULL;
    for (int i = 0; i < process->mmaps_count; i++)
    {
        if (process->mmaps[i].type == VMTYPE_HEAP)
        {
            block = &process->mmaps[i];
            break;
        }
    }

    if (block == NULL)
    {
        mos_warn("heap_control called but no heap block found");
        return 0;
    }

    switch (op)
    {
        case HEAP_GET_BASE: return block->vm.vaddr;
        case HEAP_GET_TOP: return block->vm.vaddr + block->vm.npages * MOS_PAGE_SIZE;
        case HEAP_SET_TOP:
        {
            if (arg < block->vm.vaddr)
            {
                mos_warn("heap_control: new top is below heap base");
                return 0;
            }

            if (arg % MOS_PAGE_SIZE)
            {
                mos_warn("heap_control: new top is not page-aligned");
                return 0;
            }

            if (arg == block->vm.vaddr + block->vm.npages * MOS_PAGE_SIZE)
                return 0; // no change

            if (arg < block->vm.vaddr + block->vm.npages * MOS_PAGE_SIZE)
            {
                mos_warn("heap_control: shrinking heap not supported yet");
                return 0;
            }

            return process_grow_heap(process, (arg - block->vm.vaddr) / MOS_PAGE_SIZE);
        }
        case HEAP_GET_SIZE: return block->vm.npages * MOS_PAGE_SIZE;
        case HEAP_GROW_PAGES:
        {
            // arg is the number of pages to grow
            // some bad guy would pass a huge value here :)
            return process_grow_heap(process, arg);
        }
        default: mos_warn("heap_control: unknown op %d", op); return 0;
    }
}

bool define_syscall(wait_for_thread)(tid_t tid)
{
    MOS_ASSERT(current_thread);
    thread_t *target = thread_get(tid);
    if (target == NULL)
        return false;

    if (target->owner != current_process)
    {
        pr_warn("wait_for_thread(%d) from process %d (%s) but thread belongs to process %d (%s)", tid, current_process->pid, current_process->name, target->owner->pid,
                target->owner->name);
        return false;
    }

    if (target->status == THREAD_STATUS_DEAD)
    {
        return true; // thread is already dead, no need to wait
    }

    current_thread->status = THREAD_STATUS_BLOCKED;
    wait_condition_t *wc = wc_wait_for_thread(target);
    bool added = thread_add_wait_condition(current_thread, wc);
    if (!added)
    {
        return false;
    }

    jump_to_scheduler();
    return true;
}

bool define_syscall(mutex_acquire)(bool *mutex)
{
    MOS_ASSERT(current_thread);

    if (*mutex == MUTEX_UNLOCKED) // TODO: this is unsafe in SMP
    {
        pr_info2("mutex_acquire: mutex is unlocked, acquired by tid %d", current_thread->tid);
        *mutex = MUTEX_LOCKED;
        return true;
    }

    pr_info2("mutex_acquire: mutex is locked, waiting for tid %d", current_thread->tid);

    wait_condition_t *wc = wc_wait_for_mutex(mutex);
    current_thread->status = THREAD_STATUS_BLOCKED;
    bool added = thread_add_wait_condition(current_thread, wc);
    if (!added)
        return false;

    jump_to_scheduler();
    return true;
}

bool define_syscall(mutex_release)(bool *mutex)
{
    MOS_ASSERT(current_thread);

    if (*mutex == MUTEX_LOCKED)
    {
        // TODO: this is unsafe in SMP
        pr_info2("mutex_release: mutex is locked, released by tid %d", current_thread->tid);
        *mutex = MUTEX_UNLOCKED;
        return true;
    }
    else
    {
        pr_warn("mutex_release called but mutex is already released");
        return false;
    }

    return false;
}
