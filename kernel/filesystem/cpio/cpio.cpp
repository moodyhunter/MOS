// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/filesystem/vfs_types.hpp"
#include "mos/filesystem/vfs_utils.hpp"
#include "mos/mm/mm.hpp"
#include "mos/mm/physical/pmm.hpp"
#include "mos/platform/platform.hpp"

#include <algorithm>
#include <mos/allocator.hpp>
#include <mos/filesystem/dentry.hpp>
#include <mos/filesystem/fs_types.h>
#include <mos/filesystem/vfs.hpp>
#include <mos/lib/structures/list.hpp>
#include <mos/lib/structures/tree.hpp>
#include <mos/misc/setup.hpp>
#include <mos/mos_global.h>
#include <mos/syslog/printk.hpp>
#include <mos_stdlib.hpp>
#include <mos_string.hpp>

#define CPIO_MODE_FILE_TYPE 0170000 // This masks the file type bits.
#define CPIO_MODE_SOCKET    0140000 // File type value for sockets.
#define CPIO_MODE_SYMLINK   0120000 // File type value for symbolic links.  For symbolic links, the link body is stored as file data.
#define CPIO_MODE_FILE      0100000 // File type value for regular files.
#define CPIO_MODE_BLOCKDEV  0060000 // File type value for block special devices.
#define CPIO_MODE_DIR       0040000 // File type value for directories.
#define CPIO_MODE_CHARDEV   0020000 // File type value for character special devices.
#define CPIO_MODE_FIFO      0010000 // File type value for named pipes or FIFOs.
#define CPIO_MODE_SUID      0004000 // SUID bit.
#define CPIO_MODE_SGID      0002000 // SGID bit.
#define CPIO_MODE_STICKY    0001000 // Sticky bit.

struct cpio_newc_header_t
{
    char magic[6] = { 0 };
    char ino[8] = { 0 };
    char mode[8] = { 0 };
    char uid[8] = { 0 };
    char gid[8] = { 0 };
    char nlink[8] = { 0 };
    char mtime[8] = { 0 };

    char filesize[8] = { 0 };
    char devmajor[8] = { 0 };
    char devminor[8] = { 0 };
    char rdevmajor[8] = { 0 };
    char rdevminor[8] = { 0 };

    char namesize[8] = { 0 };
    char check[8] = { 0 };
};

MOS_STATIC_ASSERT(sizeof(cpio_newc_header_t) == 110, "cpio_newc_header has wrong size");

struct cpio_inode_t : mos::NamedType<"CPIO.Inode">
{
    inode_t inode;
    size_t header_offset;
    size_t name_offset, name_length;
    size_t data_offset;
    cpio_newc_header_t header;
};

extern const inode_ops_t cpio_dir_inode_ops;
extern const inode_ops_t cpio_file_inode_ops;
extern const file_ops_t cpio_file_ops;
extern const inode_cache_ops_t cpio_icache_ops;
extern const superblock_ops_t cpio_sb_ops;
extern const file_ops_t cpio_noop_file_ops = { 0 };

static size_t initrd_read(void *buf, size_t size, size_t offset)
{
    if (unlikely(offset + size > platform_info->initrd_npages * MOS_PAGE_SIZE))
        mos_panic("initrd_read: out of bounds");
    memcpy(buf, (void *) (pfn_va(platform_info->initrd_pfn) + offset), size);
    return size;
}

static file_type_t cpio_modebits_to_filetype(u32 modebits)
{
    file_type_t type = FILE_TYPE_UNKNOWN;
    switch (modebits & CPIO_MODE_FILE_TYPE)
    {
        case CPIO_MODE_FILE: type = FILE_TYPE_REGULAR; break;
        case CPIO_MODE_DIR: type = FILE_TYPE_DIRECTORY; break;
        case CPIO_MODE_SYMLINK: type = FILE_TYPE_SYMLINK; break;
        case CPIO_MODE_CHARDEV: type = FILE_TYPE_CHAR_DEVICE; break;
        case CPIO_MODE_BLOCKDEV: type = FILE_TYPE_BLOCK_DEVICE; break;
        case CPIO_MODE_FIFO: type = FILE_TYPE_NAMED_PIPE; break;
        case CPIO_MODE_SOCKET: type = FILE_TYPE_SOCKET; break;
        default: mos_warn("invalid cpio file mode"); break;
    }

    return type;
}

static bool cpio_read_metadata(const char *target, cpio_newc_header_t *header, size_t *header_offset, size_t *name_offset, size_t *name_length, size_t *data_offset,
                               size_t *data_length)
{
    if (unlikely(strcmp(target, "TRAILER!!!") == 0))
        mos_panic("what the heck are you doing?");

    size_t offset = 0;

    while (true)
    {
        initrd_read(header, sizeof(cpio_newc_header_t), offset);

        if (strncmp(header->magic, "07070", 5) != 0 || (header->magic[5] != '1' && header->magic[5] != '2'))
        {
            mos_warn("invalid cpio header magic, possibly corrupt archive");
            return false;
        }

        offset += sizeof(cpio_newc_header_t);

        const size_t filename_len = strntoll(header->namesize, NULL, 16, sizeof(header->namesize) / sizeof(char));

        char filename[filename_len + 1]; // +1 for null terminator
        initrd_read(filename, filename_len, offset);
        filename[filename_len] = '\0';

        bool found = strncmp(filename, target, filename_len) == 0;

        if (found)
        {
            *header_offset = offset - sizeof(cpio_newc_header_t);
            *name_offset = offset;
            *name_length = filename_len;
        }

        if (unlikely(strcmp(filename, "TRAILER!!!") == 0))
            return false;

        offset += filename_len;
        offset = ((offset + 3) & ~0x03); // align to 4 bytes

        size_t data_len = strntoll(header->filesize, NULL, 16, sizeof(header->filesize) / sizeof(char));
        if (found)
        {
            *data_offset = offset;
            *data_length = data_len;
        }

        offset += data_len;
        offset = ((offset + 3) & ~0x03); // align to 4 bytes (again)

        if (found)
            return true;
    }

    MOS_UNREACHABLE();
}

should_inline cpio_inode_t *CPIO_INODE(inode_t *inode)
{
    return container_of(inode, cpio_inode_t, inode);
}

// ============================================================================================================

static cpio_inode_t *cpio_inode_trycreate(const char *path, superblock_t *sb)
{
    cpio_newc_header_t header = {};
    size_t header_offset = 0;
    size_t name_offset = 0, name_length = 0;
    size_t data_offset = 0, data_length = 0;
    const bool found = cpio_read_metadata(path, &header, &header_offset, &name_offset, &name_length, &data_offset, &data_length);
    if (!found)
        return NULL;

    cpio_inode_t *cpio_inode = mos::create<cpio_inode_t>();
    cpio_inode->header = header;
    cpio_inode->header_offset = header_offset;
    cpio_inode->name_offset = name_offset;
    cpio_inode->name_length = name_length;
    cpio_inode->data_offset = data_offset;

    const u32 modebits = strntoll(cpio_inode->header.mode, NULL, 16, sizeof(cpio_inode->header.mode) / sizeof(char));
    const u64 ino = strntoll(cpio_inode->header.ino, NULL, 16, sizeof(cpio_inode->header.ino) / sizeof(char));
    const file_type_t file_type = cpio_modebits_to_filetype(modebits & CPIO_MODE_FILE_TYPE);

    inode_t *const inode = &cpio_inode->inode;
    inode_init(inode, sb, ino, file_type);

    // 0000777 - The lower 9 bits specify read/write/execute permissions for world, group, and user following standard POSIX conventions.
    inode->perm = modebits & PERM_MASK;
    inode->size = data_length;
    inode->uid = strntoll(cpio_inode->header.uid, NULL, 16, sizeof(cpio_inode->header.uid) / sizeof(char));
    inode->gid = strntoll(cpio_inode->header.gid, NULL, 16, sizeof(cpio_inode->header.gid) / sizeof(char));
    inode->sticky = modebits & CPIO_MODE_STICKY;
    inode->suid = modebits & CPIO_MODE_SUID;
    inode->sgid = modebits & CPIO_MODE_SGID;
    inode->nlinks = strntoll(cpio_inode->header.nlink, NULL, 16, sizeof(cpio_inode->header.nlink) / sizeof(char));
    inode->ops = file_type == FILE_TYPE_DIRECTORY ? &cpio_dir_inode_ops : &cpio_file_inode_ops;
    inode->file_ops = file_type == FILE_TYPE_DIRECTORY ? &cpio_noop_file_ops : &cpio_file_ops;
    inode->cache.ops = &cpio_icache_ops;

    return cpio_inode;
}

// ============================================================================================================

static PtrResult<dentry_t> cpio_mount(filesystem_t *fs, const char *dev_name, const char *mount_options)
{
    if (unlikely(mount_options) && strlen(mount_options) > 0)
        mos_warn("cpio: mount options are not supported");

    if (dev_name && strcmp(dev_name, "none") != 0)
        pr_warn("cpio: mount: dev_name is not supported");

    superblock_t *sb = mos::create<superblock_t>();
    sb->ops = &cpio_sb_ops;

    cpio_inode_t *i = cpio_inode_trycreate(".", sb);
    if (!i)
    {
        delete sb;
        return -ENOENT; // not found
    }

    pr_dinfo2(cpio, "cpio header: %.6s", i->header.magic);
    sb->fs = fs;
    sb->root = dentry_get_from_parent(sb, NULL);
    dentry_attach(sb->root, &i->inode);
    sb->root->superblock = i->inode.superblock = sb;
    return sb->root;
}

static bool cpio_i_lookup(inode_t *parent_dir, dentry_t *dentry)
{
    // keep prepending the path with the parent path, until we reach the root
    const auto path_str = dentry_path(dentry, parent_dir->superblock->root);
    const char *path = path_str->c_str() + 1; // skip the first slash

    cpio_inode_t *inode = cpio_inode_trycreate(path, parent_dir->superblock);
    if (!inode)
        return false; // not found

    dentry_attach(dentry, &inode->inode);
    return true;
}

static void cpio_i_iterate_dir(dentry_t *dentry, vfs_listdir_state_t *state, dentry_iterator_op add_record)
{
    dentry_t *d_parent = dentry_parent(*dentry);
    if (d_parent == NULL)
        d_parent = root_dentry;

    MOS_ASSERT(d_parent->inode != NULL);
    MOS_ASSERT(dentry->inode);

    add_record(state, dentry->inode->ino, ".", FILE_TYPE_DIRECTORY);
    add_record(state, d_parent->inode->ino, "..", FILE_TYPE_DIRECTORY);

    cpio_inode_t *inode = CPIO_INODE(dentry->inode);

    char path_prefix[inode->name_length + 1]; // +1 for null terminator
    initrd_read(path_prefix, inode->name_length, inode->name_offset);
    path_prefix[inode->name_length] = '\0';
    size_t prefix_len = strlen(path_prefix); // +1 for the slash

    if (strcmp(path_prefix, ".") == 0)
        path_prefix[0] = '\0', prefix_len = 0; // root directory

    // find all children of this directory, that starts with 'path' and doesn't have any more slashes
    cpio_newc_header_t header;
    size_t offset = 0;

    while (true)
    {
        initrd_read(&header, sizeof(cpio_newc_header_t), offset);
        offset += sizeof(cpio_newc_header_t);

        if (strncmp(header.magic, "07070", 5) != 0 || (header.magic[5] != '1' && header.magic[5] != '2'))
            mos_panic("invalid cpio header magic, possibly corrupt archive");

        const size_t filename_len = strntoll(header.namesize, NULL, 16, sizeof(header.namesize) / sizeof(char));

        char filename[filename_len + 1]; // +1 for null terminator
        initrd_read(filename, filename_len, offset);
        filename[filename_len] = '\0';

        const bool found = strncmp(path_prefix, filename, prefix_len) == 0     // make sure the path starts with the parent path
                           && (prefix_len == 0 || filename[prefix_len] == '/') // make sure it's a child, not a sibling (/path/to vs /path/toooo)
                           && strchr(filename + prefix_len + 1, '/') == NULL;  // make sure it's only one level deep

        const bool is_TRAILER = strcmp(filename, "TRAILER!!!") == 0;
        const bool is_root_dot = strcmp(filename, ".") == 0;

        if (found && !is_TRAILER && !is_root_dot)
        {
            pr_dinfo2(cpio, "prefix '%s' filename '%s'", path_prefix, filename);

            const u32 modebits = strntoll(header.mode, NULL, 16, sizeof(header.mode) / sizeof(char));
            const file_type_t type = cpio_modebits_to_filetype(modebits & CPIO_MODE_FILE_TYPE);
            const s64 ino = strntoll(header.ino, NULL, 16, sizeof(header.ino) / sizeof(char));

            const char *name = filename + prefix_len + (prefix_len == 0 ? 0 : 1);          // +1 for the slash if it's not the root
            const size_t name_len = filename_len - prefix_len - (prefix_len == 0 ? 0 : 1); // -1 for the slash if it's not the root

            add_record(state, ino, mos::string(name, name_len), type);
        }

        if (unlikely(is_TRAILER))
            break;

        offset += filename_len;
        offset = ALIGN_UP(offset, 4); // align to 4 bytes

        const size_t data_len = strntoll(header.filesize, NULL, 16, sizeof(header.filesize) / sizeof(char));
        offset += data_len;
        offset = ALIGN_UP(offset, 4); // align to 4 bytes (again)
    }
}

static size_t cpio_i_readlink(dentry_t *dentry, char *buffer, size_t buflen)
{
    cpio_inode_t *inode = CPIO_INODE(dentry->inode);
    return initrd_read(buffer, std::min(buflen, inode->inode.size), inode->data_offset);
}

static bool cpio_sb_drop_inode(inode_t *inode)
{
    delete CPIO_INODE(inode);
    return true;
}

const superblock_ops_t cpio_sb_ops = {
    .drop_inode = cpio_sb_drop_inode,
};

const inode_ops_t cpio_dir_inode_ops = {
    .iterate_dir = cpio_i_iterate_dir,
    .lookup = cpio_i_lookup,
};

const inode_ops_t cpio_file_inode_ops = {
    .readlink = cpio_i_readlink,
};

const file_ops_t cpio_file_ops = {
    .read = vfs_generic_read,
};

PtrResult<phyframe_t> cpio_fill_cache(inode_cache_t *cache, uint64_t pgoff)
{
    inode_t *i = cache->owner;
    cpio_inode_t *cpio_i = CPIO_INODE(i);

    phyframe_t *page = mm_get_free_page();
    if (!page)
        return -ENOMEM;

    pmm_ref_one(page);

    if ((size_t) pgoff * MOS_PAGE_SIZE >= i->size)
        return page; // EOF, no need to read anything

    const size_t bytes_to_read = std::min((size_t) MOS_PAGE_SIZE, i->size - pgoff * MOS_PAGE_SIZE);
    const size_t read = initrd_read((char *) phyframe_va(page), bytes_to_read, cpio_i->data_offset + pgoff * MOS_PAGE_SIZE);
    MOS_ASSERT(read == bytes_to_read);
    return page;
}

const inode_cache_ops_t cpio_icache_ops = {
    .fill_cache = cpio_fill_cache,
};

static FILESYSTEM_DEFINE(fs_cpiofs, "cpiofs", cpio_mount, NULL);
FILESYSTEM_AUTOREGISTER(fs_cpiofs);
