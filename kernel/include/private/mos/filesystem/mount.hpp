// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/filesystem/vfs_types.hpp"

#include <mos/shared_ptr.hpp>

extern list_head vfs_mountpoint_list;

mos::shared_ptr<mount_t> dentry_get_mount(const dentry_t *dentry);

dentry_t *dentry_root_get_mountpoint(dentry_t *dentry);
