// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/device/block.h"
#include "mos/filesystem/filesystem.h"
#include "mos/types.h"

typedef struct _mountpoint mountpoint_t;
typedef struct _filesystem
{
    const char *name;

    bool (*op_mount)(blockdev_t *dev, fsnode_t *path);
    bool (*op_unmount)(mountpoint_t *mp);

    bool (*op_open)(const mountpoint_t *mp, const fsnode_t *path, file_open_flags mode, file_t *file);
    bool (*op_close)(file_t *file);

    size_t (*op_read)(blockdev_t *dev, file_t *file, void *buf, size_t size);
    size_t (*op_write)(blockdev_t *dev, file_t *file, const void *buf, size_t size);

    bool (*op_stat)(const mountpoint_t *mp, const fsnode_t *path, file_stat_t *stat);
    bool (*op_readlink)(const mountpoint_t *mp, const fsnode_t *path, char *buf, size_t bufsize);
} filesystem_t;

typedef struct _mountpoint
{
    mos_refcount_t refcount;
    fsnode_t *path;
    filesystem_t *fs;
    blockdev_t *dev;
    void *fs_data;
    size_t children_count;

    mountpoint_t *parent;
    mountpoint_t **children;
} mountpoint_t;

mountpoint_t *kmount(fsnode_t *path, filesystem_t *fs, blockdev_t *blockdev);
bool kunmount(mountpoint_t *mountpoint);

mountpoint_t *kmount_find_mp(const fsnode_t *path);
mountpoint_t *kmount_find_submp(mountpoint_t *mp, const fsnode_t *path);
