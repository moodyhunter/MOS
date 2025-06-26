// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/device/timer.hpp"
#include "mos/ipc/ipc_io.hpp"
#include "mos/ipc/memfd.hpp"
#include "mos/ipc/pipe.hpp"
#include "mos/kmod/kmod.hpp"
#include "mos/misc/power.hpp"
#include "mos/mm/dma.hpp"
#include "mos/tasks/signal.hpp"

#include <bits/posix/iovec.h>
#include <errno.h>
#include <fcntl.h>
#include <mos/filesystem/fs_types.h>
#include <mos/filesystem/vfs.hpp>
#include <mos/io/io.hpp>
#include <mos/locks/futex.hpp>
#include <mos/mm/mmap.hpp>
#include <mos/mm/paging/paging.hpp>
#include <mos/platform/platform.hpp>
#include <mos/syscall/decl.h>
#include <mos/syscall/number.h>
#include <mos/syslog/printk.hpp>
#include <mos/tasks/elf.hpp>
#include <mos/tasks/process.hpp>
#include <mos/tasks/schedule.hpp>
#include <mos/tasks/task_types.hpp>
#include <mos/tasks/thread.hpp>
#include <mos/types.hpp>
#include <mos_stdlib.hpp>
#include <mos_string.hpp>
#include <sys/poll.h>

#define DEFINE_SYSCALL(ret, name)                                                                                                                                        \
    MOS_STATIC_ASSERT(SYSCALL_DEFINED(name));                                                                                                                            \
    ret define_syscall(name)

// taken from mlibc
constexpr static int days_from_civil(int y, unsigned m, unsigned d) noexcept
{
    y -= m <= 2;
    const int era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = static_cast<unsigned>(y - era * 400);            // [0, 399]
    const unsigned doy = (153 * (m > 2 ? m - 3 : m + 9) + 2) / 5 + d - 1; // [0, 365]
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;           // [0, 146096]
    return era * 146097 + static_cast<int>(doe) - 719468;
}

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

DEFINE_SYSCALL(fd_t, vfs_openat)(fd_t dirfd, const char *path, u64 flags)
{
    if (path == NULL)
        return -1;

    auto f = vfs_openat(dirfd, path, (open_flags) flags);
    if (f.isErr())
        return f.getErr();
    return process_attach_ref_fd(current_process, f.get(), FD_FLAGS_NONE);
}

DEFINE_SYSCALL(long, vfs_fstatat)(fd_t fd, const char *path, file_stat_t *stat_buf, u64 flags)
{
    return vfs_fstatat(fd, path, stat_buf, (fstatat_flags) flags);
}

DEFINE_SYSCALL(size_t, io_read)(fd_t fd, void *buf, size_t count)
{
    if (buf == NULL)
        return -EFAULT;

    IO *io = process_get_fd(current_process, fd);
    if (!io)
        return -EBADF;

    return io->read(buf, count);
}

DEFINE_SYSCALL(size_t, io_write)(fd_t fd, const void *buf, size_t count)
{
    if (buf == NULL)
    {
        pr_warn("io_write called with invalid arguments (fd=%d, buf=%p, count=%zd)", fd, buf, count);
        return -EFAULT;
    }

    IO *io = process_get_fd(current_process, fd);
    if (!io)
    {
        pr_warn("io_write called with invalid fd %d", fd);
        return -EBADF;
    }

    return io->write(buf, count);
}

DEFINE_SYSCALL(bool, io_close)(fd_t fd)
{
    process_detach_fd(current_process, fd);
    return true;
}

DEFINE_SYSCALL([[noreturn]] void, exit)(u32 exit_code)
{
    // only use the lower 8 bits
    exit_code &= 0xff;
    process_exit(std::move(current_process), exit_code, 0);
    __builtin_unreachable();
}

DEFINE_SYSCALL(void, yield_cpu)(void)
{
    spinlock_acquire(&current_thread->state_lock);
    reschedule();
}

DEFINE_SYSCALL(pid_t, fork)(void)
{
    const auto parent = current_process;
    const auto child = process_do_fork(parent);
    if (unlikely(!child))
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

    mos::vector<mos::string> argv_vec;
    for (size_t i = 0; argv[i] != NULL; i++)
        argv_vec.push_back(argv[i]);

    mos::vector<mos::string> envp_vec;
    for (size_t i = 0; envp[i] != NULL; i++)
        envp_vec.push_back(envp[i]);

    const auto process = elf_create_process(path, current_process, argv_vec, envp_vec, &stdio);

    if (!process)
        return -1;

    return process->pid;
}

DEFINE_SYSCALL(tid_t, create_thread)(const char *name, thread_entry_t entry, void *arg, size_t stack_size, void *stack)
{
    const auto opt_thread = thread_new(current_process, THREAD_MODE_USER, name, stack_size, stack);
    if (opt_thread.isErr())
        return -1;

    const auto thread = opt_thread.get();

    platform_context_setup_child_thread(thread, entry, arg);
    thread_complete_init(thread);
    scheduler_add_thread(thread);
    return thread->tid;
}

DEFINE_SYSCALL(tid_t, get_tid)(void)
{
    return current_thread->tid;
}

DEFINE_SYSCALL([[noreturn]] void, thread_exit)(void)
{
    thread_exit(std::move(current_thread));
    __builtin_unreachable();
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
    auto io = ipc_create(name, max_pending_connections);
    if (io.isErr())
        return io.getErr();
    return process_attach_ref_fd(current_process, io.get(), FD_FLAGS_NONE);
}

DEFINE_SYSCALL(fd_t, ipc_accept)(fd_t listen_fd)
{
    IO *server = process_get_fd(current_process, listen_fd);
    if (server == NULL)
        return -1;

    auto client_io = ipc_accept(server);
    if (client_io.isErr())
        return client_io.getErr();

    return process_attach_ref_fd(current_process, client_io.get(), FD_FLAGS_NONE);
}

DEFINE_SYSCALL(fd_t, ipc_connect)(const char *server, size_t buffer_size)
{
    auto io = ipc_connect(server, buffer_size);
    if (io.isErr())
        return io.getErr();
    return process_attach_ref_fd(current_process, io.get(), FD_FLAGS_NONE);
}

DEFINE_SYSCALL(u64, arch_syscall)(u64 syscall, u64 arg1, u64 arg2, u64 arg3, u64 arg4)
{
    return platform_arch_syscall(syscall, arg1, arg2, arg3, arg4);
}

DEFINE_SYSCALL(long, vfs_mount)(const char *device, const char *mountpoint, const char *fs_type, const char *options)
{
    return vfs_mount(device, mountpoint, fs_type, options).getErr();
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
    return vfs_mkdir(path).getErr();
}

DEFINE_SYSCALL(size_t, vfs_list_dir)(fd_t fd, char *buffer, size_t buffer_size)
{
    IO *io = process_get_fd(current_process, fd);
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
            const FDFlags flags = (FDFlag) (u64) arg;
            if (flags.test_inverse(FD_FLAGS_CLOEXEC))
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

DEFINE_SYSCALL(void *, mmap_anonymous)(ptr_t hint_addr, size_t size, mem_perm_t perm, u64 flags)
{
    const VMFlags vmflags = VM_USER | (VMFlag) perm; // VMFlags shares the same values as mem_perm_t
    const size_t n_pages = ALIGN_UP_TO_PAGE(size) / MOS_PAGE_SIZE;

    ptr_t result = mmap_anonymous(current_mm, hint_addr, (mmap_flags_t) flags, vmflags, n_pages);
    return (void *) result;
}

DEFINE_SYSCALL(void *, mmap_file)(ptr_t hint_addr, size_t size, mem_perm_t perm, u64 mmap_flags, fd_t fd, off_t offset)
{
    const VMFlags vmflags = VM_USER | (VMFlag) perm; // VMFlags shares the same values as mem_perm_t
    const size_t n_pages = ALIGN_UP_TO_PAGE(size) / MOS_PAGE_SIZE;

    IO *io = process_get_fd(current_process, fd);
    if (io == NULL)
        return NULL;

    ptr_t result = mmap_file(current_mm, hint_addr, (mmap_flags_t) mmap_flags, vmflags, n_pages, io, offset);
    return (void *) result;
}

DEFINE_SYSCALL(pid_t, wait_for_process)(pid_t pid, u32 *exit_code, u64 flags)
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
    IO *io = process_get_fd(current_process, fd);
    if (io == NULL)
        return -1;
    return io->seek(offset, whence);
}

DEFINE_SYSCALL(off_t, io_tell)(fd_t fd)
{
    IO *io = process_get_fd(current_process, fd);
    if (io == NULL)
        return -1;
    return io->tell();
}

DEFINE_SYSCALL(bool, signal_register)(signal_t sig, const sigaction_t *action)
{
    return process_register_signal_handler(current_process, sig, action);
}

DEFINE_SYSCALL(long, signal_process)(pid_t pid, signal_t sig)
{
    const auto process = process_get(pid);
    if (!process)
        return -ESRCH;
    return signal_send_to_process(*process, sig);
}

DEFINE_SYSCALL(long, signal_thread)(tid_t tid, signal_t sig)
{
    Thread *thread = thread_get(tid);
    if (!thread)
        return -ESRCH;
    return signal_send_to_thread(thread, sig);
}

DEFINE_SYSCALL([[noreturn]] void, signal_return)(void *sp)
{
    platform_restore_from_signal_handler(sp);
    __builtin_unreachable(); // should never return
}

DEFINE_SYSCALL(bool, vm_protect)(void *addr, size_t size, mem_perm_t perm)
{
    return vm_protect(current_mm, (ptr_t) addr, size, (VMFlag) perm);
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

DEFINE_SYSCALL(long, execveat)(fd_t dirfd, const char *path, const char *const argv[], const char *const envp[], u64 flags)
{
    return process_do_execveat(dirfd, path, argv, envp, flags);
}

DEFINE_SYSCALL(long, clock_msleep)(u64 ms)
{
    timer_msleep(ms);
    return 0;
}

DEFINE_SYSCALL(fd_t, io_dup)(fd_t fd)
{
    IO *io = process_get_fd(current_process, fd);
    if (io == NULL)
        return -EBADF; // fd is not a valid file descriptor
    return process_attach_ref_fd(current_process, io->ref(), current_process->files[fd].flags);
}

DEFINE_SYSCALL(fd_t, io_dup2)(fd_t oldfd, fd_t newfd)
{
    fd_type *old = &current_process->files[oldfd];
    if (old->io == NULL)
        return -EBADF; // oldfd is not a valid file descriptor

    if (oldfd == newfd)
        return newfd;

    process_detach_fd(current_process, newfd);

    current_process->files[newfd].io = old->io->ref();
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

DEFINE_SYSCALL(long, pipe)(fd_t *reader, fd_t *writer, u64 flags)
{
    auto pipe = pipe_create(MOS_PAGE_SIZE * 4);
    if (pipe.isErr())
        return pipe.getErr();

    auto pipeio = pipeio_create(pipe.get());
    *reader = process_attach_ref_fd(current_process, &pipeio->io_r, (FDFlag) flags);
    *writer = process_attach_ref_fd(current_process, &pipeio->io_w, (FDFlag) flags);
    return 0;
}

DEFINE_SYSCALL(ssize_t, io_readv)(fd_t fd, const struct iovec *iov, int iovcnt)
{
    if (fd < 0)
        return -EBADF;

    if (iov == NULL)
        return -EFAULT;

    IO *io = process_get_fd(current_process, fd);
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
        size_t ret = io->read(iov[i].iov_base, iov[i].iov_len);
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
#ifdef __x86_64__
    timeval_t tv;
    platform_get_time(&tv);
    if (tv.day == 0)
        return -ENOTSUP;

    const auto days = days_from_civil(tv.year, tv.month, tv.day);
    ts->tv_sec = (days * 86400) + (tv.hour * 60 * 60) + (tv.minute * 60) + tv.second;
    ts->tv_nsec = 0;
#elifdef __riscv
    constexpr auto NSEC_PER_SEC = 1'000'000'000;
    u64 timestamp = 0;
    platform_get_unix_timestamp(&timestamp);
    ts->tv_sec = timestamp / NSEC_PER_SEC;
    ts->tv_nsec = timestamp % NSEC_PER_SEC;
#endif
    return 0;
}

DEFINE_SYSCALL(long, thread_setname)(tid_t tid, const char *name)
{
    Thread *thread = thread_get(tid);
    if (!thread)
        return -ESRCH;

    thread->name = name;
    return true;
}

DEFINE_SYSCALL(ssize_t, thread_getname)(tid_t tid, char *buf, size_t buflen)
{
    Thread *thread = thread_get(tid);

    if (!thread)
        return -ESRCH;

    char *end = strncpy(buf, thread->name.data(), buflen);
    return end - buf;
}

DEFINE_SYSCALL(long, vfs_fchmodat)(fd_t dirfd, const char *path, int mode, u64 flags)
{
    return vfs_fchmodat(dirfd, path, mode, flags);
}

DEFINE_SYSCALL(long, io_pread)(fd_t fd, void *buf, size_t count, off_t offset)
{
    if (fd < 0)
        return -EBADF;

    if (buf == NULL)
        return -EFAULT;

    IO *io = process_get_fd(current_process, fd);
    if (!io)
        return -EBADF;

    return io->pread(buf, count, offset);
}

DEFINE_SYSCALL(fd_t, memfd_create)(const char *name, u64 flags)
{
    auto io = memfd_create(name);
    if (io.isErr())
        return io.getErr();

    return process_attach_ref_fd(current_process, io.get(), (FDFlag) flags);
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

DEFINE_SYSCALL(long, vfs_fsync)(fd_t fd, bool data_only)
{
    IO *io = process_get_fd(current_process, fd);
    if (!io)
        return -EBADF;

    if (io->io_type != IO_FILE)
        return -EBADF;

    return vfs_fsync(io, data_only, 0, (off_t) -1);
}

DEFINE_SYSCALL(long, vfs_rmdir)(const char *path)
{
    return vfs_rmdir(path).getErr();
}

DEFINE_SYSCALL(long, kmod_load)(const char *path)
{
    const auto ret = mos::kmods::load(path);
    if (ret.isErr())
        return ret.getErr();
    return 0;
}
