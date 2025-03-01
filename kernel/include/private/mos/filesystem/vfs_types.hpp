// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/lib/sync/mutex.hpp"
#include "mos/mm/mm.hpp"
#include "mos/mm/physical/pmm.hpp"

#include <abi-bits/stat.h>
#include <cstddef>
#include <mos/allocator.hpp>
#include <mos/filesystem/fs_types.h>
#include <mos/hashmap.hpp>
#include <mos/io/io.hpp>
#include <mos/io/io_types.h>
#include <mos/lib/structures/hashmap.hpp>
#include <mos/lib/structures/list.hpp>
#include <mos/lib/structures/tree.hpp>
#include <mos/lib/sync/spinlock.hpp>
#include <mos/platform/platform.hpp>
#include <mos/string.hpp>
#include <mos/types.hpp>

MOS_ENUM_FLAGS(open_flags, OpenFlags);
MOS_ENUM_FLAGS(fstatat_flags, FStatAtFlags);

#define FILESYSTEM_DEFINE(var, fsname, mountfn, unmountfn)                                                                                                               \
    filesystem_t var = {                                                                                                                                                 \
        .name = fsname,                                                                                                                                                  \
        .mount = mountfn,                                                                                                                                                \
        .unmount = unmountfn,                                                                                                                                            \
    }

#define FILESYSTEM_AUTOREGISTER(fs)                                                                                                                                      \
    static void __register_##fs()                                                                                                                                        \
    {                                                                                                                                                                    \
        vfs_register_filesystem(&fs);                                                                                                                                    \
    }                                                                                                                                                                    \
    MOS_INIT(VFS, __register_##fs)

struct dentry_t;
typedef struct _inode_cache inode_cache_t;
struct inode_t;
struct mount_t;
struct superblock_t;
struct filesystem_t;
struct BasicFile;

struct vfs_listdir_entry_t : mos::NamedType<"VFS.ListDir.Entry">
{
    as_linked_list;
    ino_t ino;
    mos::string name;
    file_type_t type;
};

struct vfs_listdir_state_t : mos::NamedType<"VFS.ListDir.State">
{
    list_head entries;
    size_t n_count;     ///< number of entries in the list
    size_t read_offset; ///< user has read up to this offset, start from this offset when reading more entries
};

typedef void(dentry_iterator_op)(vfs_listdir_state_t *state, u64 ino, mos::string_view name, file_type_t type);

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
    /// remove a file name, this is called after nlinks is decremented
    bool (*unlink)(inode_t *dir, dentry_t *dentry);
} inode_ops_t;

typedef struct
{
    bool (*open)(inode_t *inode, BasicFile *file, bool created);                         ///< called when a file is opened, or created
    ssize_t (*read)(const BasicFile *file, void *buf, size_t size, off_t offset);        ///< read from the file
    ssize_t (*write)(const BasicFile *file, const void *buf, size_t size, off_t offset); ///< write to the file
    void (*release)(BasicFile *file);                                                    ///< called when the last reference to the file is dropped
    off_t (*seek)(BasicFile *file, off_t offset, io_seek_whence_t whence);               ///< seek to a new position in the file
    bool (*mmap)(BasicFile *file, vmap_t *vmap, off_t offset);                           ///< map the file into memory
    bool (*munmap)(BasicFile *file, vmap_t *vmap, bool *unmapped);                       ///< unmap the file from memory
} file_ops_t;

typedef struct
{
    bool (*drop_inode)(inode_t *inode); ///< The inode has zero links and the last reference to the file has been dropped
    long (*sync_inode)(inode_t *inode); ///< flush the inode to disk
} superblock_ops_t;

struct superblock_t final : mos::NamedType<"superblock">
{
    dentry_t *root;
    filesystem_t *fs;
    const superblock_ops_t *ops;
};

struct dentry_t final : mos::NamedType<"dentry">
{
    as_tree;
    spinlock_t lock;
    atomic_t refcount;
    inode_t *inode;
    mos::string name;         // for a mounted root, this is EMPTY
    superblock_t *superblock; // The superblock of the dentry
    bool is_mountpoint;
};

extern dentry_t *root_dentry;

inline mos::string dentry_name(const dentry_t *dentry)
{
    static const mos::string root_name = "<root>";
    static const mos::string null_name = "<NULL>";
    const auto name = (dentry)->name;
    return mos::string(name.value_or(dentry == root_dentry ? root_name : null_name));
}

typedef struct _inode_cache_ops
{
    /**
     * @brief Read a page from the underlying storage, at file offset pgoff * MOS_PAGE_SIZE
     */
    PtrResult<phyframe_t> (*fill_cache)(inode_cache_t *cache, uint64_t pgoff);

    bool (*page_write_begin)(inode_cache_t *cache, off_t file_offset, size_t inpage_size, phyframe_t **page_out, void **data);
    void (*page_write_end)(inode_cache_t *cache, off_t file_offset, size_t inpage_size, phyframe_t *page, void *data);

    /**
     * @brief Flush a page to the underlying storage
     */
    long (*flush_page)(inode_cache_t *cache, uint64_t pgoff, phyframe_t *page);
} inode_cache_ops_t;

typedef struct _inode_cache
{
    mutex_t lock;
    inode_t *owner;
    mos::HashMap<size_t, phyframe_t *> pages; // page index -> phyframe_t *
    const inode_cache_ops_t *ops;
} inode_cache_t;

struct inode_t final : mos::NamedType<"inode">
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
    void *private_data;         // private data
    inode_cache_t cache;        // page cache for this inode

    atomic_t refcount; ///< number of references to this inode
};

struct filesystem_t final : mos::NamedType<"filesystem">
{
    as_linked_list;
    mos::string name;
    PtrResult<dentry_t> (*mount)(filesystem_t *fs, const char *dev_name, const char *mount_options);
    void (*unmount)(filesystem_t *fs, dentry_t *mountpoint); // called when the mountpoint is unmounted
};

struct mount_t final : mos::NamedType<"mount">
{
    as_linked_list;
    dentry_t *root;       // root of the mounted tree
    dentry_t *mountpoint; // where the tree is mounted
    superblock_t *superblock;
    filesystem_t *fs;
};

struct BasicFile : IO
{
    dentry_t *dentry;
    spinlock_t offset_lock; // protects the offset field
    size_t offset;          // tracks the current position in the file
    void *private_data;

    ~BasicFile() = default;

    BasicFile(IOFlags flags, io_type_t type, dentry_t *dentry) : IO(flags, type), dentry(dentry), offset(0), private_data(nullptr)
    {
    }

    mos::string name() const override;

    const file_ops_t *get_ops() const
    {
        if (!dentry)
            goto error;

        if (!dentry->inode)
            goto error;

        if (!dentry->inode->file_ops)
            goto error;

        return dentry->inode->file_ops;

    error:
        mWarn << "no file_ops for file " << this;
        return NULL;
    }
};

struct File final : BasicFile, mos::NamedType<"File">
{
    File(IOFlags flags, dentry_t *dentry) : BasicFile(flags, IO_FILE, dentry) {};
    ~File() = default;

    size_t on_read(void *buf, size_t size) override;
    size_t on_write(const void *buf, size_t size) override;
    void on_closed() override;
    off_t on_seek(off_t offset, io_seek_whence_t whence) override;
    bool on_mmap(vmap_t *vmap, off_t offset) override;
    bool on_munmap(vmap_t *vmap, bool *unmapped) override;
};

struct Directory final : BasicFile, mos::NamedType<"Directory">
{
    Directory(IOFlags flags, dentry_t *dentry) : BasicFile(flags, IO_DIR, dentry) {};
    ~Directory() = default;

    size_t on_read(void *buf, size_t size) override;
    void on_closed() override;
};
