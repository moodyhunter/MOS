// SPDX-License-Identifier: GPL-3.0-or-later
// Virtual File System Public API

#pragma once

#include "lib/sync/mutex.h"
#include "mos/filesystem/fs_types.h"
#include "mos/tasks/task_types.h"
#include "mos/types.h"

#define get_fsdata(file, type) ((type *) file->fsdata)

typedef enum
{
    FILE_OPEN_READ = IO_READABLE,  // 1 << 0
    FILE_OPEN_WRITE = IO_WRITABLE, // 1 << 1
    FILE_OPEN_SYMLINK_NO_FOLLOW = 1 << 2,
    FILE_CREATE_IF_NOT_EXIST = 1 << 3,
} file_open_flags;

always_inline void file_format_perm(file_mode_t perms, char buf[10])
{
    buf[0] = perms.owner & FILE_PERM_READ ? 'r' : '-';
    buf[1] = perms.owner & FILE_PERM_WRITE ? 'w' : '-';
    buf[2] = perms.owner & FILE_PERM_EXEC ? 'x' : '-';
    buf[3] = perms.group & FILE_PERM_READ ? 'r' : '-';
    buf[4] = perms.group & FILE_PERM_WRITE ? 'w' : '-';
    buf[5] = perms.group & FILE_PERM_EXEC ? 'x' : '-';
    buf[6] = perms.other & FILE_PERM_READ ? 'r' : '-';
    buf[7] = perms.other & FILE_PERM_WRITE ? 'w' : '-';
    buf[8] = perms.other & FILE_PERM_EXEC ? 'x' : '-';
    buf[9] = '\0';
}

file_t *vfs_open(const char *path, file_open_flags flags);
bool vfs_stat(const char *path, file_stat_t *restrict stat);
const char *vfs_readlink(const char *path);

bool vfs_mount(const char *device, const char *path, const char *fs, const char *options);
