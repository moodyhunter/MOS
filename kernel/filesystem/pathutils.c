// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/filesystem/pathutils.h"

#include "lib/string.h"
#include "lib/structures/tree.h"
#include "mos/filesystem/fs_types.h"
#include "mos/mm/kmalloc.h"
#include "mos/printk.h"

void path_treeop_decrement_refcount(const tree_node_t *node)
{
    dentry_t *path = tree_entry(node, dentry_t);
    mos_debug(fs, "decreasing refcount of path '%s'", path->name);
    path->refcount--;
}

void path_treeop_increment_refcount(const tree_node_t *node)
{
    dentry_t *path = tree_entry(node, dentry_t);
    mos_debug(fs, "incrementing refcount of path '%s'", path->name);
    path->refcount++;
}
