// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "mos/filesystem/vfs_types.h"

#include <mos/lib/structures/list.h>
#include <stdarg.h>

typedef struct _sysfs_file sysfs_file_t;

typedef struct
{
    const char *name;
    bool (*show)(sysfs_file_t *file);
} sysfs_item_t;

// clang-format off
#define SYSFS_RO_ITEM(_name, _show_fn) { .name = _name, .show = _show_fn }
#define SYSFS_END_ITEM { 0 }
// clang-format on

#define SYSFS_ITEM_RO_STRING(name, value)                                                                                                                                \
    static bool name(sysfs_file_t *file)                                                                                                                                 \
    {                                                                                                                                                                    \
        sysfs_printf(file, "%s\n", value);                                                                                                                               \
        return true;                                                                                                                                                     \
    }

typedef struct
{
    as_linked_list;
    const char *name;
    const sysfs_item_t *const items; // must be NULL-terminated
} sysfs_dir_t;

void sysfs_register(sysfs_dir_t *entry);

__printf(2, 3) ssize_t sysfs_printf(sysfs_file_t *file, const char *fmt, ...);
