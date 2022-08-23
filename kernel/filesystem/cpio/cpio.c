// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/filesystem/cpio/cpio.h"

#include "lib/string.h"
#include "mos/filesystem/file.h"
#include "mos/filesystem/filesystem.h"
#include "mos/filesystem/fs_fwd.h"
#include "mos/filesystem/mount.h"
#include "mos/printk.h"

bool cpio_mount(blockdev_t *dev, path_t *path)
{
    MOS_UNUSED(path);
    size_t read = 0;
    cpio_newc_header_t header;
    read = dev->read(dev, &header, sizeof(header), 0);
    if (read != sizeof(header))
    {
        mos_warn("failed to read cpio archive");
        return false;
    }

    if (strncmp(header.magic, "070701", 6) != 0)
    {
        mos_warn("invalid cpio archive format, only the new ASCII format is supported");
        return false;
    }

    return true;
}

bool cpio_unmount(mountpoint_t *mountpoint)
{
    MOS_ASSERT(mountpoint->fs == &fs_cpio);
    return true;
}

bool cpio_open(mountpoint_t *mp, path_t *path, file_t **file, file_open_mode mode)
{
    return true;
}

bool cpio_read(file_t *file, void *buf, size_t size, size_t *bytes_read)
{
    return true;
}

bool cpio_close(file_t *file)
{
    return true;
}

bool cpio_stat(mountpoint_t *m, path_t *path, file_stat_t *stat)
{
    return true;
}

filesystem_t fs_cpio = {
    .name = "cpio",
    .op_mount = cpio_mount,
    .op_unmount = cpio_unmount,

    .op_open = cpio_open,
    .op_read = cpio_read,
    .op_close = cpio_close,

    .op_stat = cpio_stat,
};
