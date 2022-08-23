// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/device/block.h"
#include "mos/filesystem/file.h"
#include "mos/filesystem/fs_fwd.h"

typedef bool (*fsop_mount_t)(blockdev_t *dev, path_t *path);
typedef bool (*fsop_unmount_t)(mountpoint_t *mp);

typedef bool (*fsop_open_t)(mountpoint_t *mp, path_t *path, file_t **file, file_open_mode mode);
typedef bool (*fsop_read_t)(file_t *file, void *buf, size_t size, size_t *size_read);
typedef bool (*fsop_close_t)(file_t *file);
typedef bool (*fsop_readdir_t)(mountpoint_t *mp, path_t *path, file_t **file);
typedef bool (*fsop_readlink_t)(mountpoint_t *mp, path_t *path, char *buf, size_t size);
typedef bool (*fsop_stat_t)(mountpoint_t *mp, path_t *path, file_stat_t *stat);

typedef struct _filesystem
{
    const char *name;
    fsop_mount_t op_mount;
    fsop_unmount_t op_unmount;

    fsop_open_t op_open;
    fsop_read_t op_read;
    fsop_close_t op_close;

    fsop_readdir_t op_readdir;
    fsop_readlink_t op_readlink;

    fsop_stat_t op_stat;
} filesystem_t;
