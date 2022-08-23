// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/filesystem/path.h"

#include "lib/string.h"
#include "mos/types.h"

static path_t p_root = {
    .name = "/",
    .parent = NULL,
};

path_t *root_path = &p_root;

bool path_has_prefix(const path_t *path, const path_t *prefix)
{
    if (path == prefix)
        return true;
    if (path->parent == NULL)
        return false; // path is root
    return path_has_prefix(path->parent, prefix);
}

bool path_compare(const path_t *path1, const path_t *path2)
{
    if (path1->name == NULL || path2->name == NULL)
        return false;
    return strcmp(path1->name, path2->name) == 0 && path_compare(path1->parent, path2->parent);
}
