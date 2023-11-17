// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/filesystem/vfs_types.h"

void inode_ref(inode_t *inode);

void inode_unref(inode_t *inode);
