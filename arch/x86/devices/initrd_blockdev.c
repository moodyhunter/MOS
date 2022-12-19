// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/x86/devices/initrd_blockdev.h"

#include "lib/string.h"

size_t initrd_read(blockdev_t *dev, void *buf, size_t size, size_t offset)
{
    initrd_blockdev_t *initrd = container_of(dev, initrd_blockdev_t, blockdev);
    size_t read = 0;
    if (offset >= initrd->memblock.size_bytes)
    {
        return 0;
    }

    size_t bytes_to_read = size;

    if (offset + bytes_to_read > initrd->memblock.size_bytes)
        bytes_to_read = initrd->memblock.size_bytes - offset;

    memcpy(buf, (void *) (initrd->memblock.address + offset), bytes_to_read);
    read = bytes_to_read;
    return read;
}
