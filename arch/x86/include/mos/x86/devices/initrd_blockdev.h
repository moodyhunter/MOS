// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/device/block.h"
#include "mos/mm/mm_types.h"
#include "mos/types.h"

typedef struct
{
    memblock_t memblock;
    blockdev_t blockdev;
} initrd_blockdev_t;

void initrd_blockdev_preinstall(initrd_blockdev_t *initrd_blockdev);

size_t initrd_read(blockdev_t *dev, void *buf, size_t size, size_t bytes_read);
size_t initrd_write(blockdev_t *dev, const void *buf, size_t size, size_t bytes_written);
