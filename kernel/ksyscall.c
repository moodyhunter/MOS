// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/ksyscall.h"

#include "mos/mos_global.h"
#include "mos/printk.h"

MOS_SYSCALL_DEFINE(fd_t, file_open)(const char *path, file_open_flags flags)
{
    MOS_UNUSED(path);
    MOS_UNUSED(flags);
    return 0;
}

MOS_SYSCALL_DEFINE(bool, file_stat)(const char *path, file_stat_t *stat)
{
    MOS_UNUSED(path);
    MOS_UNUSED(stat);
    return 0;
}

MOS_SYSCALL_DEFINE(size_t, io_read)(fd_t fd, void *buf, size_t count, size_t offset)
{
    MOS_UNUSED(fd);
    MOS_UNUSED(buf);
    MOS_UNUSED(count);
    MOS_UNUSED(offset);
    return 0;
}

MOS_SYSCALL_DEFINE(size_t, io_write)(fd_t fd, const void *buf, size_t count, size_t offset)
{
    MOS_UNUSED(fd);
    MOS_UNUSED(buf);
    MOS_UNUSED(count);
    MOS_UNUSED(offset);
    return 0;
}

MOS_SYSCALL_DEFINE(bool, io_close)(fd_t fd)
{
    MOS_UNUSED(fd);
    return 0;
}

MOS_WARNING_PUSH
MOS_WARNING_DISABLE("-Wpedantic")

MOS_SYSCALL_DEFINE(int, panic)(void)
{
    mos_panic("Kernel panic called by syscall");
    return 0;
}

MOS_WARNING_POP
