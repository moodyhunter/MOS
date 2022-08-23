// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/types.h"

typedef enum
{
    FILE_OPEN_READ = 1 << 0,
    FILE_OPEN_WRITE = 1 << 1,
    FILE_OPEN_NO_FOLLOW = 1 << 2,
} file_open_flags;

typedef struct
{
    u64 accessed;
    u64 created;
    u64 modified;
} file_stat_time_t;

typedef struct _file_stat
{
    u64 dev;
    u64 ino;
    u64 mode;
    u64 nlink;
    u64 uid;
    u64 gid;
    u64 rdev;
    u64 size;
    u64 blksize;
    u64 blocks;
    file_stat_time_t time;
} file_stat_t;

typedef enum
{
    FILE_TYPE_FILE,
    FILE_TYPE_DIRECTORY,
    FILE_TYPE_SYMLINK,
    FILE_TYPE_CHAR_DEVICE,
    FILE_TYPE_BLOCK_DEVICE,
    FILE_TYPE_NAMED_PIPE,
    FILE_TYPE_SOCKET,
    FILE_TYPE_UNKNOWN,
} file_type_t;

typedef enum
{
    FILE_PERM_READ = 1 << 2,
    FILE_PERM_WRITE = 1 << 1,
    FILE_PERM_EXECUTE = 1 << 0,
} file_perm_t;

typedef struct
{
    file_perm_t owner;
    file_perm_t group;
    file_perm_t other;
} __packed file_permissions_t;

typedef struct _file
{
    file_type_t type;
    file_permissions_t permissions;
    uid_t uid;
    gid_t gid;
    bool sticky;
    bool suid;
    bool sgid;
    void *fsdata;
} file_t;

#define get_fsdata(file, type) ((type *) file->fsdata)

file_t *file_open(const char *path, file_open_flags mode);
