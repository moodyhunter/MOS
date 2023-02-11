// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "lib/structures/list.h"
#include "lib/structures/tree.h"
#include "lib/sync/mutex.h"
#include "lib/sync/spinlock.h"
#include "mos/io/io.h"
#include "mos/mos_global.h"
#include "mos/platform/platform.h"
#include "mos/types.h"

#define PATH_DELIM     '/'
#define PATH_DELIM_STR "/"
#define PATH_MAX       1024
#define FD_CWD         -69

typedef struct
{
    u64 accessed;
    u64 created;
    u64 modified;
} file_stat_time_t;

typedef enum
{
    FILE_TYPE_REGULAR,
    FILE_TYPE_DIRECTORY,
    FILE_TYPE_SYMLINK,
    FILE_TYPE_CHAR_DEVICE,
    FILE_TYPE_BLOCK_DEVICE,
    FILE_TYPE_NAMED_PIPE,
    FILE_TYPE_SOCKET,
    FILE_TYPE_UNKNOWN,
} file_type_t;

typedef struct
{
    bool read, write, execute;
} __packed file_single_perm_t;

typedef struct
{
    file_single_perm_t owner;
    file_single_perm_t group;
    file_single_perm_t others;
} file_perm_t;

typedef struct
{
    file_type_t type;
    file_perm_t perm;
    uid_t uid;
    gid_t gid;
    bool sticky;
    bool suid;
    bool sgid;
    size_t size;
} file_stat_t;

typedef struct _dentry dentry_t;
typedef struct _inode inode_t;
typedef struct _mount mount_t;
typedef struct _superblock superblock_t;
typedef struct _filesystem filesystem_t;
typedef struct _file file_t;

typedef u64 dev_t;

typedef struct
{
    bool (*write_inode)(inode_t *, bool should_sync); // write inode to disk

    // this method is called by the VFS when an inode is marked dirty. This is specifically for the inode itself being marked dirty, not its data. If
    // the update needs to be persisted by fdatasync(), then I_DIRTY_DATASYNC will be set in the flags argument. I_DIRTY_TIME will be set in the flags
    // in case lazytime is enabled and inode_t has times updated since the last ->dirty_inode call.
    bool (*inode_dirty)(inode_t *, int flags);
    void (*release_superblock)(superblock_t *); // "called when the VFS wishes to free the superblock (i.e. unmount). This is called with the superblock lock held"
} superblock_ops_t;

typedef struct
{
    bool (*lookup)(inode_t *dir, dentry_t *dentry);                                                 // lookup a file in a directory
    bool (*create)(inode_t *dir, dentry_t *dentry, file_type_t type, file_perm_t perm);             // create a new file
    bool (*link)(dentry_t *old_dentry, inode_t *dir, dentry_t *new_dentry);                         // create a hard link
    bool (*symlink)(inode_t *dir, dentry_t *dentry, const char *symname);                           // create a symbolic link
    bool (*unlink)(inode_t *dir, dentry_t *dentry);                                                 // remove a file
    bool (*mkdir)(inode_t *dir, dentry_t *dentry, file_perm_t perm);                                // create a new directory
    bool (*rmdir)(inode_t *dir, dentry_t *dentry);                                                  // remove a directory
    bool (*mknod)(inode_t *dir, dentry_t *dentry, file_perm_t perm, dev_t dev);                     // create a new device file
    bool (*rename)(inode_t *old_dir, dentry_t *old_dentry, inode_t *new_dir, dentry_t *new_dentry); // rename a file
    size_t (*readlink)(dentry_t *dentry, char *buffer, size_t buflen);                              // read the contents of a symbolic link
} inode_ops_t;

typedef struct
{
    dentry_t *(*mount)(filesystem_t *fs, const char *dev_name, const char *mount_options);
    // void (*release_superblock)(superblock_t *sb);
} filesystem_ops_t;

typedef struct
{
    bool (*open)(inode_t *inode, file_t *file);
    ssize_t (*read)(file_t *file, void *buf, size_t size);
    ssize_t (*write)(file_t *file, const void *buf, size_t size);
    int (*flush)(file_t *file);
    int (*mmap)(file_t *file, void *addr, size_t size, vmblock_t *vmblock);
} file_ops_t;

typedef struct _superblock
{
    bool dirty;
    dentry_t *root;
    list_node_t mounts;
    const superblock_ops_t *ops;
} superblock_t;

typedef struct _dentry
{
    as_tree;
    spinlock_t lock;
    atomic_t refcount;
    inode_t *inode;
    const char *name;
    superblock_t *superblock; // The root of the dentry tree
    bool is_mountpoint;
    void *private; // fs-specific data
} dentry_t;

typedef struct _inode
{
    u64 ino;                    // inode number
    file_stat_t stat;           // type, permissions, uid, gid, sticky, suid, sgid, size
    file_stat_time_t times;     // accessed, created, modified
    superblock_t *superblock;   // superblock of this inode
    ssize_t nlinks;             // number of hard links to this inode
    const inode_ops_t *ops;     // operations on this inode
    const file_ops_t *file_ops; // operations on files of this inode
    void *private;              // private data
} inode_t;

typedef struct _filesystem
{
    as_linked_list;
    const filesystem_ops_t *ops;
    const char *name;
    list_node_t superblocks;
} filesystem_t;

typedef struct _mount
{
    dentry_t *root;       // root of the mounted tree
    dentry_t *mountpoint; // where the tree is mounted
    superblock_t *superblock;
} mount_t;

typedef struct _process process_t; // forward declaration

typedef struct _file
{
    io_t io; // refcount is tracked by the io_t
    dentry_t *dentry;
    mutex_t offset_lock; // protects the offset field
    off_t offset;        // tracks the current position in the file
} file_t;

should_inline const file_ops_t *file_get_ops(file_t *file)
{
    return file->dentry->inode->file_ops;
}
