// SPDX-License-Identifier: GPL-3.0-or-later

#include "include/mos/tasks/thread.h"
#include "lib/string.h"
#include "mos/elf/elf.h"
#include "mos/filesystem/filesystem.h"
#include "mos/ipc/ipc.h"
#include "mos/ipc/ipc_types.h"
#include "mos/locks/futex.h"
#include "mos/mm/kmalloc.h"
#include "mos/mm/shm.h"
#include "mos/mos_global.h"
#include "mos/platform/platform.h"
#include "mos/printk.h"
#include "mos/syscall/decl.h"
#include "mos/tasks/process.h"
#include "mos/tasks/schedule.h"
#include "mos/tasks/task_types.h"
#include "mos/tasks/wait.h"
#include "mos/types.h"

void define_syscall(poweroff)(bool reboot, u32 magic)
{
#define POWEROFF_MAGIC MOS_FOURCC('G', 'B', 'y', 'e')
    if (current_process->effective_uid == 0)
    {
        if (magic != POWEROFF_MAGIC)
            mos_warn("poweroff syscall called with wrong magic number (0x%x)", magic);
        if (!reboot)
            platform_shutdown();
        else
            mos_warn("reboot is not implemented yet");
    }
    else
        mos_warn("poweroff syscall called by non-root user");
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
        return -1;
    if (offset)
    {
        mos_warn("offset is not supported yet");
        return -1;
    }
    io_t *io = process_get_fd(current_process, fd);
    if (!io)
        return -1;
    return io_read(io, buf, count);
}

size_t define_syscall(io_write)(fd_t fd, const void *buf, size_t count, size_t offset)
{
    if (fd < 0 || buf == NULL)
    {
        pr_warn("io_write called with invalid arguments (fd=%d, buf=%p, count=%zd, offset=%zd)", fd, buf, count, offset);
        return -1;
    }
    if (offset)
    {
        mos_warn("offset is not supported yet");
        return -1;
    }
    io_t *io = process_get_fd(current_process, fd);
    if (!io)
    {
        pr_warn("io_write called with invalid fd %d", fd);
        return -1;
    }
    return io_write(io, buf, count);
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
    const pid_t pid = current_process->pid;
    if (unlikely(pid == 1))
        mos_panic("init process exited with code %d", exit_code);

    process_handle_exit(current_process, exit_code);
    reschedule();
    MOS_UNREACHABLE();
}

void define_syscall(yield_cpu)(void)
{
    reschedule();
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

pid_t define_syscall(get_pid)(void)
{
    return current_process->pid;
}

pid_t define_syscall(get_parent_pid)(void)
{
    return current_process->parent->pid;
}

pid_t define_syscall(spawn)(const char *path, int argc, const char *const argv[])
{
    process_t *current = current_process;

    const char **new_argv = kmalloc(sizeof(uintptr_t) * (argc + 1));
    if (new_argv == NULL)
        return -1;

    for (int i = 0; i < argc; i++)
        new_argv[i] = strdup(argv[i]);
    new_argv[argc] = NULL;

    process_t *process = elf_create_process(path, current, current->terminal, current->effective_uid, (argv_t){ .argc = argc, .argv = new_argv });
    if (process == NULL)
        return -1;

    return process->pid;
}

tid_t define_syscall(create_thread)(const char *name, thread_entry_t entry, void *arg)
{
    thread_t *thread = thread_new(current_process, THREAD_MODE_USER, name, entry, arg);
    if (thread == NULL)
        return -1;

    return thread->tid;
}

tid_t define_syscall(get_tid)(void)
{
    return current_thread->tid;
}

noreturn void define_syscall(thread_exit)(void)
{
    thread_handle_exit(current_thread);
    reschedule();
    MOS_UNREACHABLE();
}

uintptr_t define_syscall(heap_control)(heap_control_op op, uintptr_t arg)
{
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
    thread_t *target = thread_get(tid);
    if (target == NULL)
        return false;

    if (target->owner != current_process)
    {
        pr_warn("wait_for_thread(%d) from process %d (%s) but thread belongs to process %d (%s)", tid, current_process->pid, current_process->name, target->owner->pid,
                target->owner->name);
        return false;
    }

    if (target->state == THREAD_STATE_DEAD)
    {
        return true; // thread is already dead, no need to wait
    }

    wait_condition_t *wc = wc_wait_for_thread(target);
    reschedule_for_wait_condition(wc);
    return true;
}

bool define_syscall(futex_wait)(futex_word_t *futex, u32 val)
{
    return futex_wait(futex, val);
}

bool define_syscall(futex_wake)(futex_word_t *futex, size_t count)
{
    return futex_wake(futex, count);
}

fd_t define_syscall(ipc_create)(const char *name, size_t max_pending_connections)
{
    io_t *io = ipc_create(name, max_pending_connections);
    if (io == NULL)
        return -1;
    return process_attach_ref_fd(current_process, io);
}

fd_t define_syscall(ipc_accept)(fd_t listen_fd)
{
    io_t *server = process_get_fd(current_process, listen_fd);
    io_t *client_io = ipc_accept(server);
    if (client_io == NULL)
        return -1;
    return process_attach_ref_fd(current_process, client_io);
}

fd_t define_syscall(ipc_connect)(const char *server, ipc_connect_flags flags, size_t buffer_size)
{
    io_t *io = ipc_connect(current_process, server, flags, buffer_size);
    if (io == NULL)
        return -1;
    return process_attach_ref_fd(current_process, io);
}
