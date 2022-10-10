// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/io/io.h"
#include "mos/ksyscall/decl.h"
#include "mos/mos_global.h"
#include "mos/platform/platform.h"
#include "mos/printk.h"
#include "mos/tasks/process.h"
#include "mos/tasks/schedule.h"
#include "mos/tasks/task_type.h"
#include "mos/tasks/thread.h"

fd_t define_ksyscall(file_open)(const char *path, file_open_flags flags)
{
    MOS_UNUSED(path);
    MOS_UNUSED(flags);
    return 0;
}

bool define_ksyscall(file_stat)(const char *path, file_stat_t *stat)
{
    MOS_UNUSED(path);
    MOS_UNUSED(stat);
    return 0;
}

size_t define_ksyscall(io_read)(fd_t fd, void *buf, size_t count, size_t offset)
{
    MOS_UNUSED(fd);
    MOS_UNUSED(buf);
    MOS_UNUSED(count);
    MOS_UNUSED(offset);
    return 0;
}

size_t define_ksyscall(io_write)(fd_t fd, const void *buf, size_t count, size_t offset)
{
    thread_t *p = current_thread;
    return io_write(p->owner->file_table[fd], buf, count);
    MOS_UNUSED(offset);
    return 0;
}

bool define_ksyscall(io_close)(fd_t fd)
{
    MOS_UNUSED(fd);
    return 0;
}

u32 define_ksyscall(panic)(void)
{
    mos_panic("Kernel panic called by syscall");
    return 0;
}

u32 define_ksyscall(exit)(u32 exit_code)
{
    pr_info("Kernel syscall exit called with code %d", exit_code);
    return 0;
}

void define_ksyscall(yield_cpu)(void)
{
    jump_to_scheduler();
}
