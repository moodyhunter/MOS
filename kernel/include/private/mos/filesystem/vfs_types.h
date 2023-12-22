// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/mm/mm.h"
#include "mos/mm/physical/pmm.h"
#include "mos/mm/slab.h"

#include <abi-bits/stat.h>
#include <mos/filesystem/fs_types.h>
#include <mos/io/io.h>
#include <mos/io/io_types.h>
#include <mos/lib/structures/hashmap.h>
#include <mos/lib/structures/list.h>
#include <mos/lib/structures/tree.h>
#include <mos/lib/sync/spinlock.h>
#include <mos/platform/platform.h>
#include <mos/types.h>

typedef struct _dentry dentry_t;
typedef struct _inode_cache inode_cache_t;
typedef struct _inode inode_t;
typedef struct _mount mount_t;
typedef struct _superblock superblock_t;
typedef struct _filesystem filesystem_t;
typedef struct _file file_t;

typedef struct
{
    as_linked_list;
    ino_t ino;
    const char *name;
    size_t name_len;
    file_type_t type;
} vfs_listdir_entry_t;

typedef struct
{
    list_head entries;
    size_t n_count;     ///< number of entries in the list
    size_t read_offset; ///< user has read up to this offset, start from this offset when reading more entries
} vfs_listdir_state_t;

typedef void(dentry_iterator_op)(vfs_listdir_state_t *state, u64 ino, const char *name, size_t name_len, file_type_t type);

typedef struct
{
    /// create a hard link
    bool (*hardlink)(dentry_t *old_dentry, inode_t *dir, dentry_t *new_dentry);
    /// iterate over the contents of a directory
    void (*iterate_dir)(dentry_t *dentry, vfs_listdir_state_t *iterator_state, dentry_iterator_op op);
    /// lookup a file in a directory, if it's unset for a directory, the VFS will use the default lookup
    bool (*lookup)(inode_t *dir, dentry_t *dentry);
    /// create a new directory
    bool (*mkdir)(inode_t *dir, dentry_t *dentry, file_perm_t perm);
    /// create a new device file
    bool (*mknode)(inode_t *dir, dentry_t *dentry, file_type_t type, file_perm_t perm, dev_t dev);
    /// create a new file
    bool (*newfile)(inode_t *dir, dentry_t *dentry, file_type_t type, file_perm_t perm);
    /// read the contents of a symbolic link
    size_t (*readlink)(dentry_t *dentry, char *buffer, size_t buflen);
    /// rename a file
    bool (*rename)(inode_t *old_dir, dentry_t *old_dentry, inode_t *new_dir, dentry_t *new_dentry);
    /// remove a directory
    bool (*rmdir)(inode_t *dir, dentry_t *dentry);
    /// create a symbolic link
    bool (*symlink)(inode_t *dir, dentry_t *dentry, const char *symname);
    /// remove a file
    bool (*unlink)(inode_t *dir, dentry_t *dentry);
} inode_ops_t;

typedef struct
{
    bool (*open)(inode_t *inode, file_t *file, bool created);
    ssize_t (*read)(const file_t *file, void *buf, size_t size, off_t offset);
    ssize_t (*write)(const file_t *file, const void *buf, size_t size, off_t offset);
    int (*flush)(file_t *file);
    void (*release)(file_t *file);
    off_t (*seek)(file_t *file, off_t offset, io_seek_whence_t whence);
    bool (*mmap)(file_t *file, vmap_t *vmap, off_t offset);
    bool (*munmap)(file_t *file, vmap_t *vmap, bool *unmapped);
} file_ops_t;

typedef struct
{
    bool (*drop_inode)(inode_t *inode);
} superblock_ops_t;

typedef struct _superblock
{
    bool dirty;
    dentry_t *root;
    filesystem_t *fs;
    list_head mounts;
    superblock_ops_t *ops;
} superblock_t;

typedef struct _dentry
{
    as_tree;
    spinlock_t lock;
    atomic_t refcount;
    inode_t *inode;
    const char *name;         // for a mounted root, this is NULL
    superblock_t *superblock; // The root of the dentry tree
    bool is_mountpoint;
    void *private; // fs-specific data
} dentry_t;

#define dentry_name(dentry)                                                                                                                                              \
    __extension__({                                                                                                                                                      \
        const char *__name = (dentry)->name;                                                                                                                             \
        __name ? __name : (dentry == root_dentry ? "<root>" : "<NULL>");                                                                                                 \
    })

typedef struct _inode_cache_ops
{
    /**
     * @brief Read a page from the underlying storage, at file offset pgoff * MOS_PAGE_SIZE
     *
     */
    phyframe_t *(*fill_cache)(inode_cache_t *cache, off_t pgoff);

    bool (*page_write_begin)(inode_cache_t *cache, off_t file_offset, size_t inpage_size, phyframe_t **page_out, void **private);
    void (*page_write_end)(inode_cache_t *cache, off_t file_offset, size_t inpage_size, phyframe_t *page, void *private);
} inode_cache_ops_t;

typedef struct _inode_cache
{
    inode_t *owner;
    hashmap_t pages; // page index -> phyframe_t *
    const inode_cache_ops_t *ops;
} inode_cache_t;

typedef struct _inode
{
    u64 ino;
    file_type_t type;
    file_perm_t perm;
    size_t size;
    uid_t uid;
    gid_t gid;
    bool sticky;
    bool suid;
    bool sgid;
    ssize_t nlinks; // number of hard links to this inode
    u64 accessed;
    u64 created;
    u64 modified;

    superblock_t *superblock;   // superblock of this inode
    const inode_ops_t *ops;     // operations on this inode
    const file_ops_t *file_ops; // operations on files of this inode
    void *private;              // private data
    inode_cache_t cache;        // page cache for this inode

    atomic_t refcount; ///< number of references to this inode
} inode_t;

typedef struct _filesystem
{
    as_linked_list;
    const char *name;
    list_head superblocks;
    dentry_t *(*mount)(filesystem_t *fs, const char *dev_name, const char *mount_options);
    void (*unmount)(filesystem_t *fs, dentry_t *mountpoint); // called when the mountpoint is unmounted
} filesystem_t;

typedef struct _mount
{
    as_linked_list;
    dentry_t *root;       // root of the mounted tree
    dentry_t *mountpoint; // where the tree is mounted
    superblock_t *superblock;
    filesystem_t *fs;
} mount_t;

typedef struct _file
{
    io_t io; // refcount is tracked by the io_t
    dentry_t *dentry;
    spinlock_t offset_lock; // protects the offset field
    size_t offset;          // tracks the current position in the file
    void *private_data;
} file_t;

extern slab_t *superblock_cache, *mount_cache, *file_cache;
