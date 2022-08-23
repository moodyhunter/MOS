// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/device/block.h"
#include "mos/filesystem/filesystem.h"
#include "mos/types.h"

typedef struct _mountpoint
{
    atomic_t refcount;
    path_t *path;
    filesystem_t *fs;
    blockdev_t *dev;
    void *fs_data;
    size_t children_count;

    mountpoint_t *parent;
    mountpoint_t **children;
} mountpoint_t;

mountpoint_t *kmount(path_t *path, filesystem_t *fs, blockdev_t *blockdev);
bool kunmount(mountpoint_t *mountpoint);

mountpoint_t *kmount_find(path_t *path);
mountpoint_t *kmount_find_under(mountpoint_t *mp, path_t *path);
