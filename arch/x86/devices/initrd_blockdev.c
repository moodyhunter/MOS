// SPDX-License-Identifier: GPL-3.0-or-later

#include <mos/x86/devices/initrd_blockdev.h>
#include <stdlib.h>
#include <string.h>

size_t initrd_read(blockdev_t *dev, void *buf, size_t size, size_t offset)
{
    initrd_blockdev_t *initrd = container_of(dev, initrd_blockdev_t, blockdev);
    const size_t initrd_size = initrd->vmblock.npages * MOS_PAGE_SIZE;

    if (offset >= initrd_size)
        return 0;

    const size_t bytes_to_read = MIN(size, initrd_size - offset);
    memcpy(buf, (void *) (initrd->vmblock.vaddr + offset), bytes_to_read);
    return bytes_to_read;
}
