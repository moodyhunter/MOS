// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/filesystem/vfs_types.h"
#include "mos/filesystem/vfs_utils.h"
#include "mos/mm/mm.h"
#include "mos/mm/physical/pmm.h"
#include "mos/mm/slab.h"
#include "mos/mm/slab_autoinit.h"
#include "mos/platform/platform.h"

#include <mos/filesystem/dentry.h>
#include <mos/filesystem/fs_types.h>
#include <mos/filesystem/vfs.h>
#include <mos/lib/structures/list.h>
#include <mos/lib/structures/tree.h>
#include <mos/mos_global.h>
#include <mos/printk.h>
#include <mos/setup.h>
#include <mos_stdlib.h>
#include <mos_string.h>

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

typedef struct
{
    char magic[6];
    char ino[8];
    char mode[8];
    char uid[8];
    char gid[8];
    char nlink[8];
    char mtime[8];

    char filesize[8];
    char devmajor[8];
    char devminor[8];
    char rdevmajor[8];
    char rdevminor[8];

    char namesize[8];
    char check[8];
} cpio_newc_header_t;

MOS_STATIC_ASSERT(sizeof(cpio_newc_header_t) == 110, "cpio_newc_header has wrong size");

static filesystem_t fs_cpiofs;

typedef struct
{
    inode_t inode;
    size_t header_offset;
    size_t name_offset, name_length;
    size_t data_offset;
    cpio_newc_header_t header;
} cpio_inode_t;

static const inode_ops_t cpio_dir_inode_ops;
static const inode_ops_t cpio_file_inode_ops;
static const file_ops_t cpio_file_ops;
static const inode_cache_ops_t cpio_icache_ops;

static slab_t *cpio_inode_cache = NULL;
SLAB_AUTOINIT("cpio_inode", cpio_inode_cache, cpio_inode_t);

static size_t initrd_read(void *buf, size_t size, size_t offset)
{
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
    cpio_newc_header_t header;
    size_t header_offset;
    size_t name_offset, name_length;
    size_t data_offset, data_length;
    const bool found = cpio_read_metadata(path, &header, &header_offset, &name_offset, &name_length, &data_offset, &data_length);
    if (!found)
        return NULL;

    cpio_inode_t *cpio_inode = kmalloc(cpio_inode_cache);
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
    inode->file_ops = file_type == FILE_TYPE_DIRECTORY ? NULL : &cpio_file_ops;
    inode->cache.ops = &cpio_icache_ops;

    return cpio_inode;
}

// ============================================================================================================

static dentry_t *cpio_mount(filesystem_t *fs, const char *dev_name, const char *mount_options)
{
    MOS_ASSERT(fs == &fs_cpiofs);

    if (unlikely(mount_options) && strlen(mount_options) > 0)
        mos_warn("cpio: mount options are not supported");

    if (dev_name && strcmp(dev_name, "none") != 0)
        pr_warn("cpio: mount: dev_name is not supported");

    superblock_t *sb = kmalloc(superblock_cache);
    cpio_inode_t *i = cpio_inode_trycreate(".", sb);
    if (!i)
    {
        kfree(sb);
        return NULL; // not found
    }

    pr_dinfo2(cpio, "cpio header: %.6s", i->header.magic);
    sb->fs = fs;
    sb->root = dentry_create(sb, NULL, NULL);
    sb->root->inode = &i->inode;
    sb->root->superblock = i->inode.superblock = sb;
    return sb->root;
}

static bool cpio_i_lookup(inode_t *parent_dir, dentry_t *dentry)
{
    // keep prepending the path with the parent path, until we reach the root
    char pathbuf[MOS_PATH_MAX_LENGTH] = { 0 };
    dentry_path(dentry, parent_dir->superblock->root, pathbuf, sizeof(pathbuf));
    const char *path = pathbuf + 1; // skip the first slash

    cpio_inode_t *inode = cpio_inode_trycreate(path, parent_dir->superblock);
    if (!inode)
        return false; // not found

    dentry->inode = &inode->inode;
    return true;
}

static void cpio_i_iterate_dir(dentry_t *dentry, vfs_listdir_state_t *state, dentry_iterator_op add_record)
{
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

            add_record(state, ino, name, name_len, type);
        }

        if (unlikely(is_TRAILER))
            break;

        offset += filename_len;
        offset = ((offset + 3) & ~0x03); // align to 4 bytes

        const size_t data_len = strntoll(header.filesize, NULL, 16, sizeof(header.filesize) / sizeof(char));
        offset += data_len;
        offset = ((offset + 3) & ~0x03); // align to 4 bytes (again)
    }
}

static size_t cpio_i_readlink(dentry_t *dentry, char *buffer, size_t buflen)
{
    cpio_inode_t *inode = CPIO_INODE(dentry->inode);
    return initrd_read(buffer, MIN(buflen, inode->inode.size), inode->data_offset);
}

static const inode_ops_t cpio_dir_inode_ops = {
    .lookup = cpio_i_lookup,
    .iterate_dir = cpio_i_iterate_dir,
};

static const inode_ops_t cpio_file_inode_ops = {
    .readlink = cpio_i_readlink,
};

static const file_ops_t cpio_file_ops = {
    .read = vfs_generic_read,
};

static phyframe_t *cpio_fill_cache(inode_cache_t *cache, off_t pgoff)
{
    inode_t *i = cache->owner;
    cpio_inode_t *cpio_i = CPIO_INODE(i);

    phyframe_t *page = mm_get_free_page();
    if (!page)
        return NULL;
    pmm_ref_one(page);
    const size_t bytes_to_read = MIN((size_t) MOS_PAGE_SIZE, i->size - pgoff * MOS_PAGE_SIZE);
    const size_t read = initrd_read((char *) phyframe_va(page), bytes_to_read, cpio_i->data_offset + pgoff * MOS_PAGE_SIZE);
    MOS_ASSERT(read == bytes_to_read);
    return page;
}

static const inode_cache_ops_t cpio_icache_ops = {
    .fill_cache = cpio_fill_cache,
};

static filesystem_t fs_cpiofs = {
    .list_node = LIST_HEAD_INIT(fs_cpiofs.list_node),
    .superblocks = LIST_HEAD_INIT(fs_cpiofs.superblocks),
    .name = "cpiofs",
    .mount = cpio_mount,
};

static void register_cpiofs(void)
{
    vfs_register_filesystem(&fs_cpiofs);
}

MOS_INIT(VFS, register_cpiofs);
