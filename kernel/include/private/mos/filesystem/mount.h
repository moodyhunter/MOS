// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/filesystem/vfs_types.h"

extern list_head vfs_mountpoint_list;

mount_t *dentry_get_mount(const dentry_t *dentry);

dentry_t *dentry_root_get_mountpoint(dentry_t *dentry);
