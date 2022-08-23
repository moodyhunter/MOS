// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/types.h"

typedef struct _blockdev
{
    const char *name;
    size_t (*read)(struct _blockdev *dev, void *buf, size_t count, size_t offset);
    size_t (*write)(struct _blockdev *dev, const void *buf, size_t count, size_t offset);
    void *data;
} blockdev_t;

void blockdev_register(blockdev_t *dev);
blockdev_t *blockdev_find(const char *name);
