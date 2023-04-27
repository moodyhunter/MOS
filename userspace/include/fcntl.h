// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <mos/filesystem/fs_types.h>
#include <mos/types.h>

fd_t open(const char *path, open_flags flags);
fd_t openat(fd_t fd, const char *path, open_flags flags);
