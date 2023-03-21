// SPDX-License-Identifier: GPL-3.0-or-later

#include <mos/device/block.h>
#include <mos/lib/structures/hashmap.h>
#include <mos/lib/structures/hashmap_common.h>
#include <mos/lib/structures/list.h>
#include <mos/mm/kmalloc.h>
#include <mos/printk.h>
#include <mos/types.h>
#include <string.h>

/**
 * @brief The hashmap of all registered block devices.
 * @note Key: Name of the block device, Value: Pointer to the block device.
 */
static hashmap_t *blockdev_map = NULL;

/**
 * @brief Register a block device
 *
 * @param dev The block device to register
 */
void blockdev_register(blockdev_t *dev)
{
    if (unlikely(blockdev_map == NULL))
    {
        MOS_ASSERT_ONCE();
        blockdev_map = kmalloc(sizeof(hashmap_t));
        memzero(blockdev_map, sizeof(hashmap_t));
        hashmap_init(blockdev_map, 64, hashmap_hash_string, hashmap_compare_string);
    }
    blockdev_t *old = hashmap_put(blockdev_map, (uintptr_t) dev->name, dev);

    if (old != NULL)
        mos_warn("blockdev %s already registered, replacing", old->name);
}

/**
 * @brief Get a block device by name
 *
 * @param name The name of the block device
 * @return blockdev_t* The pointer to such a block device, or NULL if nothing can be found.
 */
blockdev_t *blockdev_find(const char *name)
{
    if (blockdev_map == NULL)
        return NULL;
    return hashmap_get(blockdev_map, (uintptr_t) name);
}
