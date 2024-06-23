// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/filesystem/vfs_types.h"

void inode_ref(inode_t *inode);

bool inode_unref(inode_t *inode);

/**
 * @brief Unlink a dentry from its parent inode
 *
 * @param dir The parent inode
 * @param dentry The dentry to unlink
 * @return true if the inode was removed, false if there was other references/links to the inode
 */
bool inode_unlink(inode_t *dir, dentry_t *dentry);
