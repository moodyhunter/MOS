// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "mos/filesystem/vfs_types.hpp"

#include <mos/lib/structures/list.hpp>

struct sysfs_file_t;

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
    const mos::string name;
    sysfs_item_type_t type;
    bool (*show)(sysfs_file_t *file);
    size_t (*store)(sysfs_file_t *file, const char *buf, size_t count, off_t offset);
    ino_t ino;

    struct
    {
        bool (*mmap)(sysfs_file_t *file, vmap_t *vmap, off_t offset);
        bool (*munmap)(sysfs_file_t *file, vmap_t *vmap, bool *unmapped);
        size_t size;
    } mem;

    as_linked_list;
    void (*dyn_iterate)(struct _sysfs_item *item, dentry_t *dentry, vfs_listdir_state_t *iterator_state, dentry_iterator_op op);
    bool (*dyn_lookup)(inode_t *parent_dir, dentry_t *dentry);
    bool (*dyn_create)(inode_t *parent_dir, dentry_t *dentry, file_type_t type, file_perm_t perm);
} sysfs_item_t;

// clang-format off
#define SYSFS_RO_ITEM(_name, _show_fn) { .name = _name, .type = SYSFS_RO, .show = _show_fn }
#define SYSFS_RW_ITEM(_name, _show_fn, _store_fn) { .name = _name, .type = SYSFS_RW, .show = _show_fn, .store = _store_fn }
#define SYSFS_WO_ITEM(_name, _store_fn) { .name = _name, .type = SYSFS_WO, .store = _store_fn }
#define SYSFS_MEM_ITEM(_name, _mmap_fn, _munmap_fn) { .name = _name, .type = SYSFS_MEM, .mem = { .mmap = _mmap_fn, .munmap = _munmap_fn } }
#define SYSFS_DYN_ITEMS(_name, _iterate_fn, _lookup_fn) { .type = SYSFS_DYN, .dyn_iterate = _iterate_fn, .dyn_lookup = _lookup_fn }
#define SYSFS_DYN_DIR(_name, _iterate_fn, _lookup_fn, _create_fn) { .type = SYSFS_DYN, .dyn_iterate = _iterate_fn, .dyn_lookup = _lookup_fn, .dyn_create = _create_fn }
// clang-format on

#define SYSFS_ITEM_RO_PRINTF(name, fmt, ...)                                                                                                                             \
    static bool name(sysfs_file_t *file)                                                                                                                                 \
    {                                                                                                                                                                    \
        sysfs_printf(file, fmt, ##__VA_ARGS__);                                                                                                                          \
        return true;                                                                                                                                                     \
    }

#define SYSFS_ITEM_RO_STRING(name, value) SYSFS_ITEM_RO_PRINTF(name, "%s\n", value)

#define SYSFS_DEFINE_DIR(sysfs_name, sysfs_items) static sysfs_dir_t __sysfs_##sysfs_name(#sysfs_name, sysfs_items, MOS_ARRAY_SIZE(sysfs_items))

struct sysfs_dir_t
{
    as_linked_list;
    mos::string name;
    sysfs_item_t *const items;
    const size_t num_items = 0;

    sysfs_dir_t(mos::string_view name, sysfs_item_t *items, size_t num_items) : name(name), items(items), num_items(num_items) {};

    dentry_t *_dentry;        ///< for internal use only
    list_head _dynamic_items; ///< for internal use only
};

/**
 * @brief Register a sysfs directory
 *
 * @param entry the sysfs directory to register
 */
void sysfs_register(sysfs_dir_t *entry);

/**
 * @brief Register an entry in a sysfs directory
 *
 * @param sysfs_dir the sysfs directory to register the item in
 * @param item the sysfs item to register
 */
void sysfs_register_file(sysfs_dir_t *sysfs_dir, sysfs_item_t *item);

/**
 * @brief Register an entry in the sysfs root directory
 *
 * @param item the sysfs item to register
 */
should_inline void sysfs_register_root_file(sysfs_item_t *item)
{
    sysfs_register_file(NULL, item);
}

void sysfs_file_set_data(sysfs_file_t *file, void *data);
void *sysfs_file_get_data(sysfs_file_t *file);
sysfs_item_t *sysfs_file_get_item(sysfs_file_t *file);

inode_t *sysfs_create_inode(file_type_t type, void *data);

__printf(2, 3) ssize_t sysfs_printf(sysfs_file_t *file, const char *fmt, ...);
ssize_t sysfs_put_data(sysfs_file_t *file, const void *data, size_t count);
