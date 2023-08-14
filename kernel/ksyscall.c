// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/misc/power.h"
#include "mos/tasks/signal.h"

#include <mos/elf/elf.h>
#include <mos/filesystem/vfs.h>
#include <mos/io/io.h>
#include <mos/ipc/ipc.h>
#include <mos/locks/futex.h>
#include <mos/mm/mmap.h>
#include <mos/mm/paging/paging.h>
#include <mos/platform/platform.h>
#include <mos/printk.h>
#include <mos/syscall/decl.h>
#include <mos/syscall/number.h>
#include <mos/tasks/process.h>
#include <mos/tasks/schedule.h>
#include <mos/tasks/task_types.h>
#include <mos/tasks/thread.h>
#include <stdlib.h>
#include <string.h>

#define DEFINE_SYSCALL(ret, name)                                                                                                                                        \
    MOS_STATIC_ASSERT(SYSCALL_DEFINED(name));                                                                                                                            \
    ret define_syscall(name)

DEFINE_SYSCALL(void, poweroff)(bool reboot, u32 magic)
{
#define POWEROFF_MAGIC MOS_FOURCC('G', 'B', 'y', 'e')
    if (magic != POWEROFF_MAGIC)
    {
        mos_warn("poweroff syscall called with wrong magic number (0x%x)", magic);
        return;
    }

    if (!reboot)
    {
        pr_info("Meow, see ya~ :3");
        power_shutdown();
    }
    else
    {
        mos_warn("reboot is not implemented yet");
    }
}

DEFINE_SYSCALL(fd_t, vfs_openat)(fd_t dirfd, const char *path, open_flags flags)
{
    if (path == NULL)
        return -1;

    file_t *f = vfs_openat(dirfd, path, flags);
    if (!f)
        return -1;
    return process_attach_ref_fd(current_process, &f->io);
}

DEFINE_SYSCALL(bool, vfs_fstatat)(fd_t fd, const char *path, file_stat_t *stat_buf, fstatat_flags flags)
{
    if (fd < 0)
        return false;

    return vfs_fstatat(fd, path, stat_buf, flags);
}

DEFINE_SYSCALL(size_t, io_read)(fd_t fd, void *buf, size_t count)
{
    if (fd < 0 || buf == NULL)
        return -1;

    io_t *io = process_get_fd(current_process, fd);
    if (!io)
        return -1;

    return io_read(io, buf, count);
}

DEFINE_SYSCALL(size_t, io_write)(fd_t fd, const void *buf, size_t count)
{
    if (fd < 0 || buf == NULL)
    {
        pr_warn("io_write called with invalid arguments (fd=%d, buf=%p, count=%zd)", fd, buf, count);
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

DEFINE_SYSCALL(bool, io_close)(fd_t fd)
{
    if (fd < 0)
        return false;
    process_detach_fd(current_process, fd);
    return true;
}

DEFINE_SYSCALL(noreturn void, exit)(u32 exit_code)
{
    const pid_t pid = current_process->pid;
    if (unlikely(pid == 1))
        mos_panic("init process exited with code %d", exit_code);

    process_handle_exit(current_process, exit_code);
}

DEFINE_SYSCALL(void, yield_cpu)(void)
{
    reschedule();
}

DEFINE_SYSCALL(pid_t, fork)(void)
{
    process_t *parent = current_process;
    process_t *child = process_handle_fork(parent);
    if (unlikely(child == NULL))
        return -1;
    return child->pid; // return 0 for child, pid for parent
}

DEFINE_SYSCALL(pid_t, exec)(const char *path, const char *const argv[])
{
    MOS_UNUSED(path);
    MOS_UNUSED(argv);
    mos_warn("exec syscall not implemented yet");
    return -1;
}

DEFINE_SYSCALL(pid_t, get_pid)(void)
{
    return current_process->pid;
}

DEFINE_SYSCALL(pid_t, get_parent_pid)(void)
{
    return current_process->parent->pid;
}

DEFINE_SYSCALL(pid_t, spawn)(const char *path, int argc, const char *const argv[])
{
    process_t *current = current_process;

    const char **new_argv = kmalloc(sizeof(ptr_t) * (argc + 2)); // +1 for path, +1 for NULL
    if (new_argv == NULL)
        return -1;

    const int real_argc = argc + 1; // +1 for path, but not including NULL
    new_argv[0] = strdup(path);
    for (int i = 0; i < argc; i++)
        new_argv[i + 1] = strdup(argv[i]);
    new_argv[real_argc] = NULL;

    const stdio_t stdio = current_stdio();
    process_t *process = elf_create_process(path, current, (argv_t){ .argc = real_argc, .argv = new_argv }, &stdio);
    if (process == NULL)
        return -1;

    return process->pid;
}

DEFINE_SYSCALL(tid_t, create_thread)(const char *name, thread_entry_t entry, void *arg)
{
    thread_t *thread = thread_new(current_process, THREAD_MODE_USER, name, entry, arg);
    if (thread == NULL)
        return -1;

    thread_setup_complete(thread);
    return thread->tid;
}

DEFINE_SYSCALL(tid_t, get_tid)(void)
{
    return current_thread->tid;
}

DEFINE_SYSCALL(noreturn void, thread_exit)(void)
{
    thread_handle_exit(current_thread);
}

DEFINE_SYSCALL(ptr_t, heap_control)(heap_control_op op, ptr_t arg)
{
    process_t *process = current_process;

    vmap_t *block = NULL;
    list_foreach(vmap_t, map, process->mm->mmaps)
    {
        if (map->content == VMAP_HEAP)
        {
            block = map;
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
        case HEAP_GET_BASE: return block->vaddr;
        case HEAP_GET_TOP: return block->vaddr + block->npages * MOS_PAGE_SIZE;
        case HEAP_SET_TOP:
        {
            if (arg < block->vaddr)
            {
                mos_warn("heap_control: new top is below heap base");
                return 0;
            }

            if (arg % MOS_PAGE_SIZE)
            {
                mos_warn("heap_control: new top is not page-aligned");
                return 0;
            }

            if (arg == block->vaddr + block->npages * MOS_PAGE_SIZE)
                return 0; // no change

            if (arg < block->vaddr + block->npages * MOS_PAGE_SIZE)
            {
                mos_warn("heap_control: shrinking heap not supported yet");
                return 0;
            }

            return process_grow_heap(process, (arg - block->vaddr) / MOS_PAGE_SIZE);
        }
        case HEAP_GET_SIZE: return block->npages * MOS_PAGE_SIZE;
        case HEAP_GROW_PAGES:
        {
            // arg is the number of pages to grow
            // some bad guy would pass a huge value here :)
            return process_grow_heap(process, arg);
        }
        default: mos_warn("heap_control: unknown op %d", op); return 0;
    }
}

DEFINE_SYSCALL(bool, wait_for_thread)(tid_t tid)
{
    return thread_wait_for_tid(tid);
}

DEFINE_SYSCALL(bool, futex_wait)(futex_word_t *futex, u32 val)
{
    return futex_wait(futex, val);
}

DEFINE_SYSCALL(bool, futex_wake)(futex_word_t *futex, size_t count)
{
    return futex_wake(futex, count);
}

DEFINE_SYSCALL(fd_t, ipc_create)(const char *name, size_t max_pending_connections)
{
    io_t *io = ipc_create(name, max_pending_connections);
    if (io == NULL)
        return -1;
    return process_attach_ref_fd(current_process, io);
}

DEFINE_SYSCALL(fd_t, ipc_accept)(fd_t listen_fd)
{
    io_t *server = process_get_fd(current_process, listen_fd);
    if (server == NULL)
        return -1;

    io_t *client_io = ipc_accept(server);
    if (client_io == NULL)
        return -1;

    return process_attach_ref_fd(current_process, client_io);
}

DEFINE_SYSCALL(fd_t, ipc_connect)(const char *server, size_t buffer_size)
{
    io_t *io = ipc_connect(server, buffer_size);
    if (io == NULL)
        return -1;
    return process_attach_ref_fd(current_process, io);
}

DEFINE_SYSCALL(u64, arch_syscall)(u64 syscall, u64 arg1, u64 arg2, u64 arg3, u64 arg4)
{
    return platform_arch_syscall(syscall, arg1, arg2, arg3, arg4);
}

DEFINE_SYSCALL(bool, vfs_mount)(const char *device, const char *mountpoint, const char *fs_type, const char *options)
{
    return vfs_mount(device, mountpoint, fs_type, options);
}

DEFINE_SYSCALL(ssize_t, vfs_readlinkat)(fd_t dirfd, const char *path, char *buf, size_t buflen)
{
    return vfs_readlinkat(dirfd, path, buf, buflen);
}

DEFINE_SYSCALL(bool, vfs_touch)(const char *path, file_type_t type, u32 mode)
{
    return vfs_touch(path, type, mode);
}

DEFINE_SYSCALL(bool, vfs_symlink)(const char *target, const char *linkpath)
{
    return vfs_symlink(target, linkpath);
}

DEFINE_SYSCALL(bool, vfs_mkdir)(const char *path)
{
    return vfs_mkdir(path);
}

DEFINE_SYSCALL(size_t, vfs_list_dir)(fd_t fd, char *buffer, size_t buffer_size)
{
    io_t *io = process_get_fd(current_process, fd);
    if (io == NULL)
        return false;
    return vfs_list_dir(io, buffer, buffer_size);
}

DEFINE_SYSCALL(void *, mmap_anonymous)(ptr_t hint_addr, size_t size, mem_perm_t perm, mmap_flags_t flags)
{
    const vm_flags vmflags = VM_USER | (vm_flags) perm; // vm_flags shares the same values as mem_perm_t
    const size_t n_pages = ALIGN_DOWN_TO_PAGE(size) / MOS_PAGE_SIZE;

    ptr_t result = mmap_anonymous(hint_addr, flags, vmflags, n_pages);
    return (void *) result;
}

DEFINE_SYSCALL(void *, mmap_file)(ptr_t hint_addr, size_t size, mem_perm_t perm, mmap_flags_t mmap_flags, fd_t fd, off_t offset)
{
    const vm_flags vmflags = VM_USER | (vm_flags) perm; // vm_flags shares the same values as mem_perm_t
    const size_t n_pages = ALIGN_DOWN_TO_PAGE(size) / MOS_PAGE_SIZE;

    io_t *io = process_get_fd(current_process, fd);
    if (io == NULL)
        return NULL;

    ptr_t result = mmap_file(hint_addr, mmap_flags, vmflags, n_pages, io, offset);
    return (void *) result;
}

DEFINE_SYSCALL(bool, wait_for_process)(pid_t pid)
{
    return process_wait_for_pid(pid);
}

DEFINE_SYSCALL(bool, munmap)(void *addr, size_t size)
{
    return munmap((ptr_t) addr, size);
}

DEFINE_SYSCALL(bool, vfs_chdir)(const char *path)
{
    return vfs_chdir(path);
}

DEFINE_SYSCALL(ssize_t, vfs_getcwd)(char *buf, size_t size)
{
    return vfs_getcwd(buf, size);
}

DEFINE_SYSCALL(off_t, io_seek)(fd_t fd, off_t offset, io_seek_whence_t whence)
{
    io_t *io = process_get_fd(current_process, fd);
    if (io == NULL)
        return -1;
    return io_seek(io, offset, whence);
}

DEFINE_SYSCALL(off_t, io_tell)(fd_t fd)
{
    io_t *io = process_get_fd(current_process, fd);
    if (io == NULL)
        return -1;
    return io_tell(io);
}

DEFINE_SYSCALL(bool, signal_register)(signal_t sig, sigaction_t *action)
{
    return process_register_signal_handler(current_process, sig, action);
}

DEFINE_SYSCALL(bool, signal_process)(pid_t pid, signal_t sig)
{
    // stub
    MOS_UNUSED(pid);
    MOS_UNUSED(sig);
    return false;
}

DEFINE_SYSCALL(bool, signal_thread)(tid_t tid, signal_t sig)
{
    thread_t *thread = thread_get(tid);
    if (thread == NULL)
        return false;
    signal_send_to_thread(thread, sig);
    return true;
}

DEFINE_SYSCALL(noreturn void, signal_return)(void *sp)
{
    signal_return(sp);
}
