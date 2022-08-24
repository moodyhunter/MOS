// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "lib/structures/tree.h"
#include "mos/filesystem/filesystem.h"

// Path string operations
const char *path_next_segment(const char *path, size_t *segment_len);

#define path_parent(path) tree_entry(tree_node(path)->parent, fsnode_t)
fsnode_t *path_construct(const char *path);

bool path_resolve(fsnode_t *cwd, const char *path, fsnode_t **resolved);

bool path_verify_prefix(const fsnode_t *path, const fsnode_t *prefix);

const char *path_to_string_relative(const fsnode_t *root, const fsnode_t *end);

// tree traversal
void path_treeop_increment_refcount(const tree_node_t *node);
