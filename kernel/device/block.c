// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/device/block.h"

#include <mos/lib/structures/list.h>
#include <string.h>

static list_head blockdev_list = LIST_HEAD_INIT(blockdev_list);

/**
 * @brief Register a block device
 *
 * @param dev The block device to register
 */
void blockdev_register(blockdev_t *dev)
{
    linked_list_init(list_node(dev));
    list_node_append(&blockdev_list, list_node(dev));
}

/**
 * @brief Get a block device by name
 *
 * @param name The name of the block device
 * @return blockdev_t* The pointer to such a block device, or NULL if nothing can be found.
 */
blockdev_t *blockdev_find(const char *name)
{
    list_foreach(blockdev_t, dev, blockdev_list)
    {
        if (strcmp(dev->name, name) == 0)
            return dev;
    }

    return NULL;
}
