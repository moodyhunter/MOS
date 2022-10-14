// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/filesystem/filesystem.h"
#include "mos/ksyscall/decl.h"
#include "mos/mos_global.h"
#include "mos/platform/platform.h"
#include "mos/printk.h"
#include "mos/tasks/schedule.h"
#include "mos/tasks/task_type.h"

fd_t define_ksyscall(file_open)(const char *path, file_open_flags flags)
{
    file_t *f = vfs_open(path, flags);
    int fd = current_thread->owner->files_count++;
    current_thread->owner->file_table[fd] = &f->io;
    return fd;
}

bool define_ksyscall(file_stat)(const char *path, file_stat_t *stat)
{
    return vfs_stat(path, stat);
}

size_t define_ksyscall(io_read)(fd_t fd, void *buf, size_t count, size_t offset)
{
    if (offset)
        mos_warn("offset is not supported yet");
    return io_read(current_thread->owner->file_table[fd], buf, count);
}

size_t define_ksyscall(io_write)(fd_t fd, const void *buf, size_t count, size_t offset)
{
    if (offset)
        mos_warn("offset is not supported yet");
    return io_write(current_thread->owner->file_table[fd], buf, count);
}

bool define_ksyscall(io_close)(fd_t fd)
{
    io_close(current_thread->owner->file_table[fd]);
    return true;
}

u32 define_ksyscall(panic)(void)
{
    mos_panic("Kernel panic called by syscall");
    return 0;
}

u32 define_ksyscall(exit)(u32 exit_code)
{
    int pid = current_thread->owner->id.process_id;
    if (unlikely(pid == 1))
        mos_panic("init process exited with code %d", exit_code);

    pr_info("Kernel syscall exit called with code %d from pid %d", exit_code, pid);
    current_thread->status = THREAD_STATUS_DEAD;
    return 0;
}

void define_ksyscall(yield_cpu)(void)
{
    jump_to_scheduler();
}
