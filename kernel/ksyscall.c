// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/device/timer.h"
#include "mos/ipc/ipc_io.h"
#include "mos/ipc/memfd.h"
#include "mos/ipc/pipe.h"
#include "mos/misc/power.h"
#include "mos/mm/dma.h"
#include "mos/tasks/signal.h"

#include <bits/posix/iovec.h>
#include <errno.h>
#include <fcntl.h>
#include <mos/filesystem/fs_types.h>
#include <mos/filesystem/vfs.h>
#include <mos/io/io.h>
#include <mos/locks/futex.h>
#include <mos/mm/mmap.h>
#include <mos/mm/paging/paging.h>
#include <mos/platform/platform.h>
#include <mos/syscall/decl.h>
#include <mos/syscall/number.h>
#include <mos/syslog/printk.h>
#include <mos/tasks/elf.h>
#include <mos/tasks/process.h>
#include <mos/tasks/schedule.h>
#include <mos/tasks/task_types.h>
#include <mos/tasks/thread.h>
#include <mos/types.h>
#include <mos_stdlib.h>
#include <mos_string.h>
#include <sys/poll.h>

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
    if (IS_ERR(f))
        return PTR_ERR(f);
    return process_attach_ref_fd(current_process, &f->io, FD_FLAGS_NONE);
}

DEFINE_SYSCALL(long, vfs_fstatat)(fd_t fd, const char *path, file_stat_t *stat_buf, fstatat_flags flags)
{
    return vfs_fstatat(fd, path, stat_buf, flags);
}

DEFINE_SYSCALL(size_t, io_read)(fd_t fd, void *buf, size_t count)
{
    if (buf == NULL)
        return -EFAULT;

    io_t *io = process_get_fd(current_process, fd);
    if (!io)
        return -EBADF;

    return io_read(io, buf, count);
}

DEFINE_SYSCALL(size_t, io_write)(fd_t fd, const void *buf, size_t count)
{
    if (buf == NULL)
    {
        pr_warn("io_write called with invalid arguments (fd=%d, buf=%p, count=%zd)", fd, buf, count);
        return -EFAULT;
    }

    io_t *io = process_get_fd(current_process, fd);
    if (!io)
    {
        pr_warn("io_write called with invalid fd %d", fd);
        return -EBADF;
    }

    return io_write(io, buf, count);
}

DEFINE_SYSCALL(bool, io_close)(fd_t fd)
{
    process_detach_fd(current_process, fd);
    return true;
}

DEFINE_SYSCALL(noreturn void, exit)(u32 exit_code)
{
    // only use the lower 8 bits
    exit_code &= 0xff;
    process_exit(current_process, exit_code, 0);
}

DEFINE_SYSCALL(void, yield_cpu)(void)
{
    spinlock_acquire(&current_thread->state_lock);
    reschedule();
}

DEFINE_SYSCALL(pid_t, fork)(void)
{
    process_t *parent = current_process;
    process_t *child = process_do_fork(parent);
    if (unlikely(child == NULL))
        return -1;
    return child->pid; // return 0 for child, pid for parent
}

DEFINE_SYSCALL(pid_t, get_pid)(void)
{
    return current_process->pid;
}

DEFINE_SYSCALL(pid_t, get_parent_pid)(void)
{
    return current_process->parent->pid;
}

DEFINE_SYSCALL(pid_t, spawn)(const char *path, const char *const argv[], const char *const envp[])
{
    const stdio_t stdio = current_stdio();
    process_t *process = elf_create_process(path, current_process, argv, envp, &stdio);

    if (process == NULL)
        return -1;

    return process->pid;
}

DEFINE_SYSCALL(tid_t, create_thread)(const char *name, thread_entry_t entry, void *arg, size_t stack_size, void *stack)
{
    thread_t *thread = thread_new(current_process, THREAD_MODE_USER, name, stack_size, stack);
    if (thread == NULL)
        return -1;

    platform_context_setup_child_thread(thread, entry, arg);
    thread_complete_init(thread);
    return thread->tid;
}

DEFINE_SYSCALL(tid_t, get_tid)(void)
{
    return current_thread->tid;
}

DEFINE_SYSCALL(noreturn void, thread_exit)(void)
{
    thread_exit(current_thread);
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
    if (IS_ERR(io))
        return PTR_ERR(io);
    return process_attach_ref_fd(current_process, io, FD_FLAGS_NONE);
}

DEFINE_SYSCALL(fd_t, ipc_accept)(fd_t listen_fd)
{
    io_t *server = process_get_fd(current_process, listen_fd);
    if (server == NULL)
        return -1;

    io_t *client_io = ipc_accept(server);
    if (IS_ERR(client_io))
        return PTR_ERR(client_io);

    return process_attach_ref_fd(current_process, client_io, FD_FLAGS_NONE);
}

DEFINE_SYSCALL(fd_t, ipc_connect)(const char *server, size_t buffer_size)
{
    io_t *io = ipc_connect(server, buffer_size);
    if (IS_ERR(io))
        return PTR_ERR(io);
    return process_attach_ref_fd(current_process, io, FD_FLAGS_NONE);
}

DEFINE_SYSCALL(u64, arch_syscall)(u64 syscall, u64 arg1, u64 arg2, u64 arg3, u64 arg4)
{
    return platform_arch_syscall(syscall, arg1, arg2, arg3, arg4);
}

DEFINE_SYSCALL(long, vfs_mount)(const char *device, const char *mountpoint, const char *fs_type, const char *options)
{
    return vfs_mount(device, mountpoint, fs_type, options);
}

DEFINE_SYSCALL(ssize_t, vfs_readlinkat)(fd_t dirfd, const char *path, char *buf, size_t buflen)
{
    return vfs_readlinkat(dirfd, path, buf, buflen);
}

DEFINE_SYSCALL(long, vfs_unlinkat)(fd_t dirfd, const char *path)
{
    return vfs_unlinkat(dirfd, path);
}

DEFINE_SYSCALL(long, vfs_symlink)(const char *target, const char *linkpath)
{
    return vfs_symlink(target, linkpath);
}

DEFINE_SYSCALL(long, vfs_mkdir)(const char *path)
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

DEFINE_SYSCALL(long, fd_manipulate)(fd_t fd, u64 op, void *arg)
{
    fd_type *fdt = &current_process->files[fd];
    if (fdt->io == NULL)
        return -EBADF;

    switch (op)
    {
        case F_DUPFD:
        {
            fd_t fd2 = process_attach_ref_fd(current_process, fdt->io, fdt->flags);
            return fd2;
        }
        case F_DUPFD_CLOEXEC:
        {
            fd_t fd2 = process_attach_ref_fd(current_process, fdt->io, fdt->flags | FD_FLAGS_CLOEXEC);
            return fd2;
        }
        case F_GETFD:
        {
            return fdt->flags;
        }
        case F_SETFD:
        {
            // test if arg is a valid flag
            u64 flags = (u64) arg;
            if (flags & ~FD_FLAGS_CLOEXEC)
                return -EINVAL;
            fdt->flags = flags;
            return 0;
        }
        case F_GETFL:
        case F_SETFL:
        {
            return -ENOSYS; // not implemented
        }
        case F_GETLK:
        case F_SETLK:
        case F_SETLKW:
        {
            return -ENOSYS; // not implemented
        }
        case F_GETOWN:
        case F_SETOWN:
        case F_GETOWN_EX:
        case F_SETOWN_EX:
        case F_GETSIG:
        case F_SETSIG:
        case F_GETOWNER_UIDS:
        case F_ADD_SEALS:
        case F_GET_SEALS:
        {
            return -ENOSYS; // not implemented
        }
    }

    return -EINVAL;
}

DEFINE_SYSCALL(void *, mmap_anonymous)(ptr_t hint_addr, size_t size, mem_perm_t perm, mmap_flags_t flags)
{
    const vm_flags vmflags = VM_USER | (vm_flags) perm; // vm_flags shares the same values as mem_perm_t
    const size_t n_pages = ALIGN_UP_TO_PAGE(size) / MOS_PAGE_SIZE;

    ptr_t result = mmap_anonymous(current_mm, hint_addr, flags, vmflags, n_pages);
    return (void *) result;
}

DEFINE_SYSCALL(void *, mmap_file)(ptr_t hint_addr, size_t size, mem_perm_t perm, mmap_flags_t mmap_flags, fd_t fd, off_t offset)
{
    const vm_flags vmflags = VM_USER | (vm_flags) perm; // vm_flags shares the same values as mem_perm_t
    const size_t n_pages = ALIGN_UP_TO_PAGE(size) / MOS_PAGE_SIZE;

    io_t *io = process_get_fd(current_process, fd);
    if (io == NULL)
        return NULL;

    ptr_t result = mmap_file(current_mm, hint_addr, mmap_flags, vmflags, n_pages, io, offset);
    return (void *) result;
}

DEFINE_SYSCALL(pid_t, wait_for_process)(pid_t pid, u32 *exit_code, u32 flags)
{
    return process_wait_for_pid(pid, exit_code, flags);
}

DEFINE_SYSCALL(bool, munmap)(void *addr, size_t size)
{
    return munmap((ptr_t) addr, size);
}

DEFINE_SYSCALL(long, vfs_chdirat)(fd_t dirfd, const char *path)
{
    return vfs_chdirat(dirfd, path);
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

DEFINE_SYSCALL(bool, signal_register)(signal_t sig, const sigaction_t *action)
{
    return process_register_signal_handler(current_process, sig, action);
}

DEFINE_SYSCALL(long, signal_process)(pid_t pid, signal_t sig)
{
    process_t *process = process_get(pid);
    if (!process)
        return -ESRCH;
    return signal_send_to_process(process, sig);
}

DEFINE_SYSCALL(long, signal_thread)(tid_t tid, signal_t sig)
{
    thread_t *thread = thread_get(tid);
    if (thread == NULL)
        return -ESRCH;
    return signal_send_to_thread(thread, sig);
}

DEFINE_SYSCALL(noreturn void, signal_return)(void *sp)
{
    platform_restore_from_signal_handler(sp);
}

DEFINE_SYSCALL(bool, vm_protect)(void *addr, size_t size, mem_perm_t perm)
{
    return vm_protect(current_mm, (ptr_t) addr, size, (vm_flags) perm);
}

DEFINE_SYSCALL(int, io_poll)(struct pollfd *fds, nfds_t nfds, int timeout)
{
    if (timeout == 0) // poll with timeout 0 is just a check
        return 0;

    if (!fds || nfds == 0)
        return -1;

    for (nfds_t i = 0; i < nfds; i++)
    {
        if (fds[i].fd < 0)
            fds[i].revents = 0;
        pr_info2("io_poll: fd=%d, events=%d", fds[i].fd, fds[i].events);
    }

    pr_emerg("io_poll is not implemented yet\n");
    signal_send_to_thread(current_thread, SIGKILL); // unimplemented
    return 0;
}

#ifndef FD_CLR
#define FD_CLR(__fd, __set) (__set->fds_bits[__fd / 8] &= ~(1 << (__fd % 8)))
#endif

#ifndef FD_ISSET
#define FD_ISSET(__fd, __set) (__set->fds_bits[__fd / 8] & (1 << (__fd % 8)))
#endif

#ifndef FD_SET
#define FD_SET(__fd, __set) (__set->fds_bits[__fd / 8] |= 1 << (__fd % 8))
#endif

#ifndef FD_ZERO
#define FD_ZERO(__set) memset(__set->fds_bits, 0, sizeof(fd_set))
#endif

DEFINE_SYSCALL(int, io_pselect)(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, const struct timespec *timeout, const sigset_t *sigmask)
{
    MOS_UNUSED(timeout);
    MOS_UNUSED(sigmask);

    for (int i = 0; i < nfds; i++)
    {
        if (readfds && FD_ISSET(i, readfds))
        {
            // pr_info2("io_pselect: fd=%d, read", i);
        }
        if (writefds && FD_ISSET(i, writefds))
        {
            // pr_info2("io_pselect: fd=%d, write", i);
        }
        if (exceptfds && FD_ISSET(i, exceptfds))
        {
            // pr_info2("io_pselect: fd=%d, except", i);
        }
    }

    return 1; // stub
}

DEFINE_SYSCALL(long, execveat)(fd_t dirfd, const char *path, const char *const argv[], const char *const envp[], u32 flags)
{
    return process_do_execveat(current_process, dirfd, path, argv, envp, flags);
}

DEFINE_SYSCALL(long, clock_msleep)(u64 ms)
{
    timer_msleep(ms);
    return 0;
}

DEFINE_SYSCALL(fd_t, io_dup)(fd_t fd)
{
    io_t *io = process_get_fd(current_process, fd);
    if (io == NULL)
        return -EBADF; // fd is not a valid file descriptor
    return process_attach_ref_fd(current_process, io_ref(io), current_process->files[fd].flags);
}

DEFINE_SYSCALL(fd_t, io_dup2)(fd_t oldfd, fd_t newfd)
{
    fd_type *old = &current_process->files[oldfd];
    if (old->io == NULL)
        return -EBADF; // oldfd is not a valid file descriptor

    if (oldfd == newfd)
        return newfd;

    process_detach_fd(current_process, newfd);

    current_process->files[newfd].io = io_ref(old->io);
    current_process->files[newfd].flags = old->flags;
    return newfd;
}

DEFINE_SYSCALL(bool, dmabuf_alloc)(size_t n_pages, ptr_t *phys, ptr_t *virt)
{
    pfn_t pfn = dmabuf_allocate(n_pages, virt);
    *phys = pfn * MOS_PAGE_SIZE;
    return !IS_ERR_VALUE(pfn);
}

DEFINE_SYSCALL(bool, dmabuf_free)(ptr_t vaddr, ptr_t paddr)
{
    return dmabuf_free(vaddr, paddr);
}

DEFINE_SYSCALL(bool, dmabuf_share)(void *buffer, size_t size, ptr_t *phyaddr)
{
    pfn_t pfn = dmabuf_share(buffer, size);

    if (IS_ERR_VALUE(pfn))
        return false;

    *phyaddr = pfn * MOS_PAGE_SIZE;
    return true;
}

DEFINE_SYSCALL(bool, dmabuf_unshare)(ptr_t phys, size_t size, void *buf)
{
    return dmabuf_unshare(phys, size, buf);
}

DEFINE_SYSCALL(long, pipe)(fd_t *reader, fd_t *writer, fd_flags_t flags)
{
    pipe_t *pipe = pipe_create(MOS_PAGE_SIZE * 4);
    if (IS_ERR(pipe))
        return PTR_ERR(pipe);

    pipeio_t *pipeio = pipeio_create(pipe);
    *reader = process_attach_ref_fd(current_process, &pipeio->io_r, flags);
    *writer = process_attach_ref_fd(current_process, &pipeio->io_w, flags);
    return 0;
}

DEFINE_SYSCALL(ssize_t, io_readv)(fd_t fd, const struct iovec *iov, int iovcnt)
{
    if (fd < 0)
        return -EBADF;

    if (iov == NULL)
        return -EFAULT;

    io_t *io = process_get_fd(current_process, fd);
    if (!io)
        return -EBADF;

    for (int i = 0; i < iovcnt; i++)
    {
        if (iov[i].iov_base == NULL)
            return -EFAULT;
    }

    ssize_t bytes_read = 0;

    for (int i = 0; i < iovcnt; i++)
    {
        size_t ret = io_read(io, iov[i].iov_base, iov[i].iov_len);
        if (IS_ERR_VALUE(ret))
            return ret;

        bytes_read += ret;

        if (ret != iov[i].iov_len)
            break; // short read, leave
    }

    return bytes_read;
}

DEFINE_SYSCALL(long, vfs_unmount)(const char *path)
{
    return vfs_unmount(path);
}

DEFINE_SYSCALL(long, clock_gettimeofday)(struct timespec *ts)
{
    timeval_t tv;
    platform_get_time(&tv);
    ts->tv_sec = tv.hour * 3600 + tv.minute * 60 + tv.second;
    ts->tv_nsec = 0;
    return 0;
}

DEFINE_SYSCALL(long, thread_setname)(tid_t tid, const char *name)
{
    thread_t *thread = thread_get(tid);
    if (thread == NULL)
        return -ESRCH;

    if (thread->name)
        kfree(thread->name);

    thread->name = strdup(name);
    return true;
}

DEFINE_SYSCALL(ssize_t, thread_getname)(tid_t tid, char *buf, size_t buflen)
{
    thread_t *thread = thread_get(tid);

    if (thread == NULL)
        return -ESRCH;

    char *end = strncpy(buf, thread->name, buflen);
    return end - buf;
}

DEFINE_SYSCALL(long, vfs_fchmodat)(fd_t dirfd, const char *path, int mode, int flags)
{
    return vfs_fchmodat(dirfd, path, mode, flags);
}

DEFINE_SYSCALL(long, io_pread)(fd_t fd, void *buf, size_t count, off_t offset)
{
    if (fd < 0)
        return -EBADF;

    if (buf == NULL)
        return -EFAULT;

    io_t *io = process_get_fd(current_process, fd);
    if (!io)
        return -EBADF;

    return io_pread(io, buf, count, offset);
}

DEFINE_SYSCALL(fd_t, memfd_create)(const char *name, u32 flags)
{
    io_t *io = memfd_create(name);
    if (IS_ERR(io))
        return PTR_ERR(io);

    return process_attach_ref_fd(current_process, io, flags);
}

DEFINE_SYSCALL(long, signal_mask_op)(int how, const sigset_t *set, sigset_t *oldset)
{
    if (oldset)
        *oldset = current_thread->signal_info.mask;

    if (set)
    {
        switch (how)
        {
            case SIG_SETMASK: current_thread->signal_info.mask = *set; break;
            case SIG_BLOCK:
            {
                char *ptr = (char *) set;
                char *mask = (char *) &current_thread->signal_info.mask;
                for (size_t i = 0; i < sizeof(sigset_t); i++)
                    mask[i] |= ptr[i];
                break;
            }
            case SIG_UNBLOCK:
            {
                char *ptr = (char *) set;
                char *mask = (char *) &current_thread->signal_info.mask;
                for (size_t i = 0; i < sizeof(sigset_t); i++)
                    mask[i] &= ~ptr[i];
                break;
            }
            default: return -EINVAL;
        }
    }

    return 0;
}
