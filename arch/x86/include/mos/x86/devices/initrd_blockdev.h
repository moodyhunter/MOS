// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/device/block.h"
#include "mos/platform/platform.h"

typedef struct
{
    uintptr_t address;
    size_t size_bytes;
    bool available;
} memregion_t;

typedef struct
{
    memregion_t memblock;
    blockdev_t blockdev;
} initrd_blockdev_t;

size_t initrd_read(blockdev_t *dev, void *buf, size_t size, size_t bytes_read);
