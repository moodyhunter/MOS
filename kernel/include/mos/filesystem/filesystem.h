// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "lib/structures/tree.h"
#include "mos/io/io.h"
#include "mos/types.h"

typedef enum
{
    FILE_OPEN_READ = IO_READABLE,
    FILE_OPEN_WRITE = IO_WRITABLE,
    FILE_OPEN_SYMLINK_NO_FOLLOW = 1 << 2,
} file_open_flags;

typedef struct
{
    u64 accessed;
    u64 created;
    u64 modified;
} file_stat_time_t;

typedef enum
{
    FILE_TYPE_REGULAR_FILE,
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
    FILE_PERM_EXEC = 1 << 0,
} file_perm_t;

typedef struct
{
    file_perm_t owner;
    file_perm_t group;
    file_perm_t other;
} __packed file_permissions_t;

always_inline void file_format_perm(file_permissions_t perms, char buf[10])
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

typedef struct
{
    file_type_t type;
    file_permissions_t permissions;
    uid_t uid;
    gid_t gid;
    bool sticky;
    bool suid;
    bool sgid;
    size_t size;
} file_stat_t;

typedef struct _fsnode
{
    as_tree;
    atomic_t refcount;
    const char *name;
} fsnode_t;

typedef struct _file
{
    io_t io;
    void *pdata;
    fsnode_t *fsnode;
} file_t;

#define PATH_SEPARATOR        '/'
#define PATH_SEPARATOR_STRING "/"

#define get_fsdata(file, type) ((type *) file->fsdata)

bool vfs_path_open(const fsnode_t *path, file_open_flags flags, file_t *file);
bool vfs_path_stat(const fsnode_t *path, file_stat_t *restrict stat);
bool vfs_path_readlink(const fsnode_t *path, fsnode_t **link);

file_t *vfs_open(const char *path, file_open_flags flags);
bool vfs_stat(const char *path, file_stat_t *restrict stat);
fsnode_t *vfs_readlink(const char *path);
