// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "lib/structures/list.h"
#include "lib/structures/tree.h"
#include "lib/sync/spinlock.h"
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
} __packed file_mode_t;

typedef struct
{
    file_type_t type;
    file_mode_t permissions;
    uid_t uid;
    gid_t gid;
    bool sticky;
    bool suid;
    bool sgid;
    size_t size;
} file_stat_t;

typedef struct _dentry dentry_t;
typedef struct _inode inode_t;
typedef struct _mountpoint mountpoint_t;
typedef struct _superblock superblock_t;
typedef struct _path path_t;

typedef u64 dev_t;

typedef struct
{
    inode_t *(*inode_create)(superblock_t *this_superblock);
    int (*inode_destroy)(inode_t *this_inode);

    void (*inode_dirty)(
        inode_t *,
        int flags); //     this method is called by the VFS when an inode is marked dirty. This is specifically for the inode itself being marked dirty, not its data. If
                    //     the update needs to be persisted by fdatasync(), then I_DIRTY_DATASYNC will be set in the flags argument. I_DIRTY_TIME will be set in the flags
                    //     in case lazytime is enabled and inode_t has times updated since the last ->dirty_inode call.

    int (*inode_write)(inode_t *, bool should_sync); // write inode to disk
    void (*inode_delete)(inode_t *);                 // delete inode from disk
    void (*put_super)(superblock_t *);               // "called when the VFS wishes to free the superblock (i.e. unmount). This is called with the superblock lock held"
    int (*sync)(superblock_t *sb);                   // called when VFS is writing out all dirty data associated with a superblock.
} superblock_ops_t;

typedef struct _superblock
{
    list_node_t all_inodes; // list of all inodes in this super block (or filesystem)
    superblock_ops_t *ops;
} superblock_t;

typedef struct
{
    dentry_t *(*lookup)(inode_t *dir, dentry_t *dentry);                                           // lookup a file in a directory
    int (*create)(inode_t *dir, dentry_t *dentry, file_mode_t mode);                               // create a new file
    int (*link)(dentry_t *old_dentry, inode_t *dir, dentry_t *new_dentry);                         // create a hard link
    int (*symlink)(inode_t *dir, dentry_t *dentry, const char *symname);                           // create a symbolic link
    int (*unlink)(inode_t *dir, dentry_t *dentry);                                                 // remove a file
    int (*mkdir)(inode_t *dir, dentry_t *dentry, file_mode_t mode);                                // create a new directory
    int (*rmdir)(inode_t *dir, dentry_t *dentry);                                                  // remove a directory
    int (*mknod)(inode_t *dir, dentry_t *dentry, file_mode_t mode, dev_t dev);                     // create a new device file
    int (*rename)(inode_t *old_dir, dentry_t *old_dentry, inode_t *new_dir, dentry_t *new_dentry); // rename a file
    int (*readlink)(dentry_t *dentry, char *buffer, size_t buflen);                                // read the contents of a symbolic link
} inode_ops_t;

struct _dentry
{
    as_tree;
    spinlock_t lock;
    atomic_t refcount;
    inode_t *inode;
    const char *name;
    superblock_t *d_sb; /* The root of the dentry tree */
    void *d_fsdata;     // fs-specific data
};

struct _inode
{
    file_stat_t stat;         // type, permissions, uid, gid, sticky, suid, sgid, size
    file_stat_time_t times;   // accessed, created, modified
    const inode_ops_t *ops;   // operations on this inode
    superblock_t *superblock; // superblock of this inode
    atomic_t refcount;
    void *private; // private data
};

struct _path
{
    mountpoint_t *mnt;
    dentry_t *dentry;
};

// struct _file
// {
//     io_t io;
//     void *pdata;
//     fsnode_t *fsnode;
// };
