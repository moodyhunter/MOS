// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "lib/containers.h"
#include "mos/io/io.h"
#include "mos/types.h"

typedef struct _filesystem filesystem_t;
typedef struct _fsnode fsnode_t;
typedef struct _file_stat file_stat_t;
typedef struct _blockdev blockdev_t;

typedef enum
{
    OPEN_READ = 1 << 0,
    OPEN_WRITE = 1 << 1,
    OPEN_SYMLINK_NO_FOLLOW = 1 << 2,
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

typedef struct _file_stat
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
    fsnode_t *fsnode;
} file_t;

#define PATH_SEPARATOR        '/'
#define PATH_SEPARATOR_STRING "/"
#define PATH_MAX_LENGTH       256

#define get_fsdata(file, type) ((type *) file->fsdata)

extern fsnode_t root_path;

bool vfs_path_open(fsnode_t *path, file_open_flags flags, file_t *file);
bool vfs_path_readlink(fsnode_t *path, fsnode_t **link);
bool vfs_path_stat(fsnode_t *path, file_stat_t *restrict stat);

file_t *vfs_open(const char *path, file_open_flags flags);
fsnode_t *vfs_readlink(const char *path);
bool vfs_stat(const char *path, file_stat_t *restrict stat);
size_t vfs_read(file_t *file, void *buf, size_t count);
size_t vfs_write(file_t *file, const void *buf, size_t count);
