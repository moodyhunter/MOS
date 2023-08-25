// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/filesystem/vfs_types.h"

void inode_init(inode_t *inode, superblock_t *sb, u64 ino, file_type_t type);
inode_t *inode_create(superblock_t *sb, u64 ino, file_type_t type);
