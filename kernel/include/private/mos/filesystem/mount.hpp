// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/filesystem/vfs_types.hpp"

#include <mos/shared_ptr.hpp>

extern mos::HashMap<ptr<dentry_t>, ptr<mount_t>> vfs_mountpoint_map; // dentry_t -> mount_t

ptr<mount_t> dentry_get_mount(const ptr<dentry_t> dentry);

ptr<dentry_t> dentry_root_get_mountpoint(const ptr<dentry_t> dentry);
