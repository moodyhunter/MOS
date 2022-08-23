// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/filesystem/fs_fwd.h"
#include "mos/types.h"

typedef struct _path
{
    const char *name;
    path_t *parent;
} path_t;

extern path_t *root_path;

bool path_has_prefix(const path_t *path, const path_t *prefix);
bool path_compare(const path_t *path1, const path_t *path2);
