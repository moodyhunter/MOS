// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "lib/structures/tree.h"
#include "mos/filesystem/fs_fwd.h"
#include "mos/types.h"

typedef struct _path
{
    as_tree;
    atomic_t refcount;
    const char *name;
} path_t;

extern path_t root_path;
extern tree_op_t path_tree_op;

path_t *construct_path(const char *path);
bool path_verify_prefix(const path_t *path, const path_t *prefix);
const char *path_get_full_path_string(const path_t *root, const path_t *end);
