// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "lib/structures/list.h"
#include "lib/structures/tree.h"
#include "lib/sync/mutex.h"
#include "lib/sync/spinlock.h"
#include "mos/io/io.h"
#include "mos/platform/platform.h"
#include "mos/types.h"

#define PATH_DELIM     '/'
#define PATH_DELIM_STR "/"
#define PATH_MAX       1024

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
typedef struct _mount mount_t;
typedef struct _superblock superblock_t;
typedef struct _filesystem filesystem_t;
typedef struct _file_ops file_ops_t;
typedef struct _file file_t;

typedef u64 dev_t;

typedef struct
{
    inode_t *(*alloc_inode)(superblock_t *this_superblock);
    int (*write_inode)(inode_t *, bool should_sync); // write inode to disk
    int (*release_inode)(inode_t *this_inode);

    // this method is called by the VFS when an inode is marked dirty. This is specifically for the inode itself being marked dirty, not its data. If
    // the update needs to be persisted by fdatasync(), then I_DIRTY_DATASYNC will be set in the flags argument. I_DIRTY_TIME will be set in the flags
    // in case lazytime is enabled and inode_t has times updated since the last ->dirty_inode call.
    void (*inode_dirty)(inode_t *, int flags);
    void (*release_superblock)(superblock_t *); // "called when the VFS wishes to free the superblock (i.e. unmount). This is called with the superblock lock held"
} superblock_ops_t;

typedef struct
{
    int (*init)(dentry_t *self);
    void (*deinit)(dentry_t *self);
    char *(*get_name)(dentry_t *self, char *buffer, size_t buflen);
} dentry_ops_t;

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

typedef struct _filesystem_ops
{
    dentry_t *(*mount)(filesystem_t *fs, const char *dev_name, const char *mount_options);
    void (*release_superblock)(superblock_t *sb);
} filesystem_ops_t;

typedef struct _file_ops
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
    superblock_ops_t *ops;
    list_node_t mounts; // ?
    dentry_ops_t *default_d_op;
} superblock_t;

typedef struct _dentry
{
    as_tree;
    spinlock_t lock;
    atomic_t refcount;
    inode_t *inode;
    const char *name;
    dentry_ops_t *ops;
    superblock_t *superblock; // The root of the dentry tree
    bool is_mountpoint;
    void *private; // fs-specific data
} dentry_t;

typedef struct _inode
{
    u64 ino;                  // inode number
    file_stat_t stat;         // type, permissions, uid, gid, sticky, suid, sgid, size
    file_stat_time_t times;   // accessed, created, modified
    const inode_ops_t *ops;   // operations on this inode
    superblock_t *superblock; // superblock of this inode
    ssize_t nlinks;           // number of hard links to this inode
    file_ops_t *file_ops;     // operations on files of this inode
    void *private;            // private data
} inode_t;

typedef struct _filesystem
{
    as_linked_list;
    const char *name;
    filesystem_ops_t *ops;
    list_node_t superblocks;
} filesystem_t;

typedef struct _mount
{
    as_linked_list;
    dentry_t *root;       // root of the mounted tree
    dentry_t *mountpoint; // where the tree is mounted
    superblock_t *superblock;
} mount_t;

typedef struct _process process_t; // forward declaration

typedef struct _file
{
    io_t io; // refcount is tracked by the io_t
    dentry_t *dentry;
    file_ops_t *ops;
    process_t *owner;

    mutex_t offset_lock; // protects the offset field
    off_t offset;        // tracks the current position in the file
} file_t;
