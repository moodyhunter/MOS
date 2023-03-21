// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <mos/device/block.h>
#include <mos/platform/platform.h>

typedef struct
{
    blockdev_t blockdev;
    vmblock_t vmblock;
} initrd_blockdev_t;

size_t initrd_read(blockdev_t *dev, void *buf, size_t size, size_t bytes_read);
