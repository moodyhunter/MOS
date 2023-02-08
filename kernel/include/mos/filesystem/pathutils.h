// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "lib/structures/tree.h"

#define path_parent(path) tree_entry(tree_node(path)->parent, fsnode_t)

// tree traversal
void path_treeop_decrement_refcount(const tree_node_t *node);
void path_treeop_increment_refcount(const tree_node_t *node);
