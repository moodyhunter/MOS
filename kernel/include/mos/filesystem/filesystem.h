// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "fs_types.h"
#include "mos/types.h"

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

#define PATH_SEPARATOR        '/'
#define PATH_SEPARATOR_STRING "/"

#define get_fsdata(file, type) ((type *) file->fsdata)

bool vfs_path_open(const fsnode_t *path, file_open_flags flags, file_t *file);
bool vfs_path_stat(const fsnode_t *path, file_stat_t *restrict stat);
bool vfs_path_readlink(const fsnode_t *path, fsnode_t **link);

file_t *vfs_open(const char *path, file_open_flags flags);
bool vfs_stat(const char *path, file_stat_t *restrict stat);
fsnode_t *vfs_readlink(const char *path);
