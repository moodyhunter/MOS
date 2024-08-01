// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/filesystem/vfs_types.h"

void inode_ref(inode_t *inode);

[[nodiscard]] bool inode_unref(inode_t *inode);

/**
 * @brief Unlink a dentry from its parent inode
 *
 * @param dir The parent inode
 * @param dentry The dentry to unlink
 * @return true if the inode was successfully unlinked, false otherwise (but the inode may still be there if other references exist)
 */
bool inode_unlink(inode_t *dir, dentry_t *dentry);
