// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "mos/filesystem/vfs_types.h"

#include <mos/lib/structures/list.h>

typedef struct _sysfs_file sysfs_file_t;

typedef enum
{
    _SYSFS_INVALID = 0,
    SYSFS_RO,
    SYSFS_RW,
    SYSFS_WO,
    SYSFS_MEM, ///< memory-backed file
    SYSFS_DYN, ///< dynamic directory items
} sysfs_item_type_t;

typedef struct _sysfs_item
{
    const char *name;
    sysfs_item_type_t type;
    bool (*show)(sysfs_file_t *file);
    bool (*store)(sysfs_file_t *file, const char *buf, size_t count, off_t offset);
    ino_t ino;

    union
    {
        struct
        {
            bool (*mmap)(sysfs_file_t *file, vmap_t *vmap, off_t offset);
            bool (*munmap)(sysfs_file_t *file, vmap_t *vmap, bool *unmapped);
            size_t size;
        } mem;

        struct
        {
            as_linked_list;
            size_t (*iterate)(struct _sysfs_item *item, dentry_t *dentry, dir_iterator_state_t *iterator_state, dentry_iterator_op op);
            bool (*lookup)(inode_t *parent_dir, dentry_t *dentry);
            bool (*create)(inode_t *parent_dir, dentry_t *dentry, file_type_t type, file_perm_t perm);
        } dyn;
    };
} sysfs_item_t;

// clang-format off
#define SYSFS_RO_ITEM(_name, _show_fn) { .name = _name, .type = SYSFS_RO, .show = _show_fn }
#define SYSFS_RW_ITEM(_name, _show_fn, _store_fn) { .name = _name, .type = SYSFS_RW, .show = _show_fn, .store = _store_fn }
#define SYSFS_WO_ITEM(_name, _store_fn) { .name = _name, .type = SYSFS_WO, .store = _store_fn }
#define SYSFS_MEM_ITEM(_name, _mmap_fn, _munmap_fn) { .name = _name, .type = SYSFS_MEM, .mem.mmap = _mmap_fn, .mem.munmap = _munmap_fn }
#define SYSFS_DYN_ITEMS(_name, _iterate_fn, _lookup_fn) { .type = SYSFS_DYN, .dyn.iterate = _iterate_fn, .dyn.lookup = _lookup_fn }
#define SYSFS_DYN_DIR(_name, _iterate_fn, _lookup_fn, _create_fn) { .type = SYSFS_DYN, .dyn.iterate = _iterate_fn, .dyn.lookup = _lookup_fn, .dyn.create = _create_fn }
// clang-format on

#define SYSFS_ITEM_RO_STRING(name, value)                                                                                                                                \
    static bool name(sysfs_file_t *file)                                                                                                                                 \
    {                                                                                                                                                                    \
        sysfs_printf(file, "%s\n", value);                                                                                                                               \
        return true;                                                                                                                                                     \
    }

#define SYSFS_DEFINE_DIR(sysfs_name, sysfs_items)                                                                                                                        \
    static sysfs_dir_t __sysfs_##sysfs_name = {                                                                                                                          \
        .list_node = LIST_NODE_INIT(__sysfs_##sysfs_name),                                                                                                               \
        .name = #sysfs_name,                                                                                                                                             \
        .items = sysfs_items,                                                                                                                                            \
        .num_items = MOS_ARRAY_SIZE(sysfs_items),                                                                                                                        \
        ._dentry = NULL,                                                                                                                                                 \
        ._dynamic_items = LIST_HEAD_INIT(__sysfs_##sysfs_name._dynamic_items),                                                                                           \
    };

typedef struct
{
    as_linked_list;
    const char *name;
    sysfs_item_t *const items;
    const size_t num_items;
    dentry_t *_dentry; ///< for internal use only
    list_head _dynamic_items;
} sysfs_dir_t;

void sysfs_register(sysfs_dir_t *entry);
void sysfs_register_file(sysfs_dir_t *sysfs_dir, sysfs_item_t *item, void *data);

should_inline void sysfs_register_root_file(sysfs_item_t *item, void *data)
{
    sysfs_register_file(NULL, item, data);
}

void sysfs_file_set_data(sysfs_file_t *file, void *data);
void *sysfs_file_get_data(sysfs_file_t *file);

inode_t *sysfs_create_inode(file_type_t type, void *data);

__printf(2, 3) ssize_t sysfs_printf(sysfs_file_t *file, const char *fmt, ...);
ssize_t sysfs_put_data(sysfs_file_t *file, const void *data, size_t count);
