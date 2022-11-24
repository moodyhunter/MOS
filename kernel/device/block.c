// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/device/block.h"

#include "lib/string.h"
#include "lib/structures/hashmap.h"
#include "lib/structures/hashmap_common.h"
#include "lib/structures/list.h"
#include "mos/mm/kmalloc.h"
#include "mos/printk.h"
#include "mos/types.h"

/**
 * \brief The hashmap of all registered block devices.
 * \note Key: Name of the block device, Value: Pointer to the block device.
 */
static hashmap_t *blockdev_map = NULL;

/**
 * \brief Get the hash of a block device
 *
 * \param key The name of the block device
 * \return hash_t The hash of that name
 */
static hash_t hashmap_blockdev_hash(const void *key)
{
    return hashmap_hash_string((const char *) key);
}

/**
 * \brief Compare two block devices
 *
 * \param key1 Name of the first block device
 * \param key2 Name of the second block device
 * \return int 0 if the names are equal, otherwise 1
 */
static int hashmap_blockdev_compare(const void *key1, const void *key2)
{
    return hashmap_compare_string((const char *) key1, (const char *) key2);
}

/**
 * \brief Register a block device
 *
 * \param dev The block device to register
 */
void blockdev_register(blockdev_t *dev)
{
    if (unlikely(blockdev_map == NULL))
    {
        MOS_ASSERT_ONCE();
        blockdev_map = kmalloc(sizeof(hashmap_t));
        memzero(blockdev_map, sizeof(hashmap_t));
        hashmap_init(blockdev_map, 64, hashmap_blockdev_hash, hashmap_blockdev_compare);
    }
    blockdev_t *old = hashmap_put(blockdev_map, dev->name, dev);

    if (old != NULL)
        mos_warn("blockdev %s already registered, replacing", old->name);
}

/**
 * \brief Get a block device by name
 *
 * \param name The name of the block device
 * \return blockdev_t* The pointer to such a block device, or NULL if nothing can be found.
 */
blockdev_t *blockdev_find(const char *name)
{
    if (blockdev_map == NULL)
        return NULL;
    return hashmap_get(blockdev_map, name);
}
