// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/filesystem/filesystem.h"
#include "mos/ksyscall/decl.h"
#include "mos/mos_global.h"
#include "mos/platform/platform.h"
#include "mos/printk.h"
#include "mos/tasks/process.h"
#include "mos/tasks/schedule.h"
#include "mos/tasks/task_type.h"

fd_t define_ksyscall(file_open)(const char *path, file_open_flags flags)
{
    file_t *f = vfs_open(path, flags);
    if (!f)
        return -1;
    return process_attach_fd(current_thread->owner, &f->io);
}

bool define_ksyscall(file_stat)(const char *path, file_stat_t *stat)
{
    return vfs_stat(path, stat);
}

size_t define_ksyscall(io_read)(fd_t fd, void *buf, size_t count, size_t offset)
{
    if (offset)
        mos_warn("offset is not supported yet");
    return io_read(current_thread->owner->files[fd], buf, count);
}

size_t define_ksyscall(io_write)(fd_t fd, const void *buf, size_t count, size_t offset)
{
    if (offset)
        mos_warn("offset is not supported yet");
    return io_write(current_thread->owner->files[fd], buf, count);
}

bool define_ksyscall(io_close)(fd_t fd)
{
    process_detach_fd(current_thread->owner, fd);
    return true;
}

noreturn void define_ksyscall(panic)(void)
{
    mos_panic("Kernel panic called by syscall");
}

noreturn void define_ksyscall(exit)(u32 exit_code)
{
    int pid = current_thread->owner->pid;
    if (unlikely(pid == 1))
        mos_panic("init process exited with code %d", exit_code);

    pr_info("Kernel syscall exit called with code %d from pid %d", exit_code, pid);
    process_handle_exit(current_thread->owner, exit_code);
    jump_to_scheduler();
    MOS_UNREACHABLE();
}

void define_ksyscall(yield_cpu)(void)
{
    jump_to_scheduler();
}

pid_t define_ksyscall(fork)(void)
{
    process_t *parent = current_thread->owner;
    process_t *child = process_handle_fork(parent);
    if (child == NULL)
        return 0;

    return current_process == child ? 0 : child->pid; // return 0 for child, pid for parent
}

pid_t define_ksyscall(exec)(const char *path, const char *const argv[])
{
    MOS_UNUSED(path);
    MOS_UNUSED(argv);

    // TODO
    return -1;
}

pid_t define_ksyscall(get_pid)()
{
    MOS_ASSERT(current_thread->owner);
    return current_thread->owner->pid;
}
