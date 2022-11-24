// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/device/block.h"

#include "lib/string.h"
#include "lib/structures/hashmap.h"
#include "lib/structures/hashmap_common.h"
#include "lib/structures/list.h"
#include "mos/mm/kmalloc.h"
#include "mos/printk.h"
#include "mos/types.h"

static hashmap_t *blockdev_map = NULL;

static hash_t hashmap_blockdev_hash(const void *key)
{
    return hashmap_hash_string((const char *) key);
}

static int hashmap_blockdev_compare(const void *key1, const void *key2)
{
    return hashmap_compare_string((const char *) key1, (const char *) key2);
}

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

blockdev_t *blockdev_find(const char *name)
{
    if (blockdev_map == NULL)
        return NULL;
    return hashmap_get(blockdev_map, name);
}
