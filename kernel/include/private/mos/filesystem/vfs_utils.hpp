// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/filesystem/vfs_types.hpp"

void inode_init(inode_t *inode, superblock_t *sb, u64 ino, file_type_t type);
inode_t *inode_create(superblock_t *sb, u64 ino, file_type_t type);

/**
 * @brief Create a new dentry with the given name and parent
 *
 * @param sb The superblock of the dentry
 * @param parent The parent dentry
 * @param name The name of the dentry
 *
 * @return The new dentry, or NULL if the dentry could not be created
 * @note The returned dentry will have its reference count of 0.
 */
dentry_t *dentry_get_from_parent(superblock_t *sb, dentry_t *parent, mos::string_view name = "");

ssize_t vfs_generic_read(const file_t *file, void *buf, size_t size, off_t offset);
ssize_t vfs_generic_write(const file_t *file, const void *buf, size_t size, off_t offset);
ssize_t vfs_generic_lseek(const file_t *file, off_t offset, int whence);
int vfs_generic_close(const file_t *file);

// ! simple page cache functions
phyframe_t *simple_fill_cache(inode_cache_t *cache, off_t pgoff);
bool simple_page_write_begin(inode_cache_t *icache, off_t offset, size_t size, phyframe_t **page, void **private_);
void simple_page_write_end(inode_cache_t *icache, off_t offset, size_t size, phyframe_t *page, void *private_);
long simple_flush_page_discard_data(inode_cache_t *icache, off_t pgoff, phyframe_t *page);

// ! simple in-memory directory iterator
void vfs_generic_iterate_dir(const dentry_t *dir, vfs_listdir_state_t *state, dentry_iterator_op op);
