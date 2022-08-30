// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "lib/structures/tree.h"
#include "mos/filesystem/filesystem.h"

#define path_parent(path) tree_entry(tree_node(path)->parent, fsnode_t)

extern fsnode_t root_path;

// Path string operations
const char *path_next_segment(const char *path, size_t *segment_len);
const char *path_to_string_relative(const fsnode_t *root, const fsnode_t *end);

// Create or find a node in the filesystem tree, with all symbolic links resolved.
fsnode_t *path_find_fsnode(const char *path);

// resolve the symbolic links in the path, given a cwd.
bool path_resolve(fsnode_t *cwd, const char *path, fsnode_t **resolved);

bool path_verify_prefix(const fsnode_t *path, const fsnode_t *prefix);

// tree traversal
void path_treeop_decrement_refcount(const tree_node_t *node);
void path_treeop_increment_refcount(const tree_node_t *node);
