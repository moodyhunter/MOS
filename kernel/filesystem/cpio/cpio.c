// SPDX-License-Identifier: GPL-3.0-or-later

#include <mos/device/block.h>
#include <mos/filesystem/dentry.h>
#include <mos/filesystem/fs_types.h>
#include <mos/kconfig.h>
#include <mos/lib/structures/list.h>
#include <mos/lib/structures/tree.h>
#include <mos/mm/kmalloc.h>
#include <mos/mos_global.h>
#include <mos/printk.h>
#include <mos/setup.h>
#include <stdlib.h>
#include <string.h>

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

typedef struct
{
    cpio_newc_header_t header;
    size_t header_offset;

    size_t name_offset;
    size_t name_length;

    size_t data_offset;
    size_t data_length;
} cpio_metadata_t;

MOS_STATIC_ASSERT(sizeof(cpio_newc_header_t) == 110, "cpio_newc_header has wrong size");

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

static bool cpio_read_metadata(blockdev_t *dev, const char *target, cpio_metadata_t *metadata)
{
    if (unlikely(strcmp(target, "TRAILER!!!") == 0))
        mos_panic("what the heck are you doing?");

    size_t offset = 0;
    cpio_newc_header_t header;

    while (true)
    {
        dev->read(dev, &header, sizeof(cpio_newc_header_t), offset);

        if (strncmp(header.magic, "07070", 5) != 0 || (header.magic[5] != '1' && header.magic[5] != '2'))
        {
            mos_warn("invalid cpio header magic, possibly corrupt archive");
            return false;
        }

        offset += sizeof(cpio_newc_header_t);

        size_t filename_len = strntoll(header.namesize, NULL, 16, sizeof(header.namesize) / sizeof(char));

        char filename[filename_len + 1]; // +1 for null terminator
        dev->read(dev, filename, filename_len, offset);
        filename[filename_len] = '\0';

        bool found = strncmp(filename, target, filename_len) == 0;

        if (found)
        {
            metadata->header = header;
            metadata->header_offset = offset - sizeof(cpio_newc_header_t);
            metadata->name_offset = offset;
            metadata->name_length = filename_len;
        }

        if (unlikely(strcmp(filename, "TRAILER!!!") == 0))
            return false;

        offset += filename_len;
        offset = ((offset + 3) & ~0x03); // align to 4 bytes

        size_t data_len = strntoll(header.filesize, NULL, 16, sizeof(header.filesize) / sizeof(char));
        if (found)
        {
            metadata->data_offset = offset;
            metadata->data_length = data_len;
        }

        offset += data_len;
        offset = ((offset + 3) & ~0x03); // align to 4 bytes (again)

        if (found)
            return true;
    }

    MOS_UNREACHABLE();
}

static void cpio_metadata_to_stat(cpio_metadata_t *metadata, file_stat_t *stat)
{
    stat->size = metadata->data_length;

    stat->uid = strntoll(metadata->header.uid, NULL, 16, sizeof(metadata->header.uid) / sizeof(char));
    stat->gid = strntoll(metadata->header.gid, NULL, 16, sizeof(metadata->header.gid) / sizeof(char));

    //  0000777  The lower 9 bits specify read/write/execute permissions for world, group, and user following standard POSIX conventions.
    u32 modebits = strntoll(metadata->header.mode, NULL, 16, sizeof(metadata->header.mode) / sizeof(char));
    stat->perm.others.read = modebits & 0004;
    stat->perm.others.write = modebits & 0002;
    stat->perm.others.execute = modebits & 0001;

    stat->perm.group.read = modebits & 0040;
    stat->perm.group.write = modebits & 0020;
    stat->perm.group.execute = modebits & 0010;

    stat->perm.owner.read = modebits & 0400;
    stat->perm.owner.write = modebits & 0200;
    stat->perm.owner.execute = modebits & 0100;

    stat->sticky = modebits & CPIO_MODE_STICKY;
    stat->sgid = modebits & CPIO_MODE_SGID;
    stat->suid = modebits & CPIO_MODE_SUID;

    stat->type = cpio_modebits_to_filetype(modebits & CPIO_MODE_FILE_TYPE);
}

// ============================================================================================================

extern filesystem_t fs_cpiofs;

typedef struct
{
    inode_t inode;
    cpio_metadata_t metadata;
} cpio_inode_t;

typedef struct
{
    superblock_t sb;
    blockdev_t *dev;
} cpio_superblock_t;

static const inode_ops_t cpio_dir_inode_ops;
static const inode_ops_t cpio_file_inode_ops;
static const file_ops_t cpio_file_ops;

should_inline cpio_superblock_t *CPIO_SB(superblock_t *sb)
{
    return container_of(sb, cpio_superblock_t, sb);
}

should_inline cpio_inode_t *CPIO_INODE(inode_t *inode)
{
    return container_of(inode, cpio_inode_t, inode);
}

// ============================================================================================================

static cpio_inode_t *cpio_inode_create(cpio_superblock_t *sb, cpio_metadata_t *metadata)
{
    cpio_inode_t *i = kzalloc(sizeof(cpio_inode_t));
    file_stat_t stat;
    cpio_metadata_to_stat(metadata, &stat);

    const s64 ino = strntoll(metadata->header.ino, NULL, 16, sizeof(metadata->header.ino) / sizeof(char));
    const s64 nlinks = strntoll(metadata->header.nlink, NULL, 16, sizeof(metadata->header.nlink) / sizeof(char));

    i->inode.ino = ino;
    i->inode.nlinks = nlinks;

    i->inode.stat = stat;
    i->inode.ops = stat.type == FILE_TYPE_DIRECTORY ? &cpio_dir_inode_ops : &cpio_file_inode_ops;
    i->inode.superblock = &sb->sb;
    i->inode.file_ops = stat.type == FILE_TYPE_DIRECTORY ? NULL : &cpio_file_ops;
    i->metadata = *metadata;

    return i;
}

// ============================================================================================================

static dentry_t *cpio_mount(filesystem_t *fs, const char *dev_name, const char *mount_options)
{
    MOS_ASSERT(fs == &fs_cpiofs);

    if (unlikely(mount_options) && strlen(mount_options) > 0)
        mos_warn("cpio: mount options are not supported");

    blockdev_t *dev = blockdev_find(dev_name);
    if (unlikely(!dev))
    {
        mos_warn("cpio: failed to find block device %s", dev_name);
        return NULL;
    }

    cpio_metadata_t header;
    const bool found = cpio_read_metadata(dev, ".", &header);
    if (!found)
        return NULL; // not found

    mos_debug(cpio, "cpio header: %.6s", header.header.magic);

    cpio_superblock_t *sb = kzalloc(sizeof(cpio_superblock_t));
    cpio_inode_t *i = cpio_inode_create(sb, &header);
    dentry_t *root = dentry_create(NULL, NULL); // root dentry has no name, this "feature" is used by cpio_i_lookup
    root->inode = &i->inode;
    sb->sb.root = root;
    sb->dev = dev;
    root->superblock = i->inode.superblock = &sb->sb;
    return root;
}

static ssize_t cpio_f_read(const file_t *file, void *buf, size_t size, off_t offset)
{
    cpio_metadata_t metadata = CPIO_INODE(file->dentry->inode)->metadata;
    blockdev_t *dev = CPIO_SB(file->dentry->inode->superblock)->dev;

    if (offset >= (long) metadata.data_length)
        return 0; // EOF

    const size_t bytes_to_read = MIN(size, metadata.data_length - offset);
    size_t read = dev->read(dev, buf, bytes_to_read, metadata.data_offset + offset);
    return read;
}

static bool cpio_i_lookup(inode_t *parent_dir, dentry_t *dentry)
{
    // keep prepending the path with the parent path, until we reach the root
    char pathbuf[MOS_PATH_MAX_LENGTH] = { 0 };
    dentry_path(dentry, parent_dir->superblock->root, pathbuf, sizeof(pathbuf));
    const char *path = pathbuf + 1; // skip the first slash

    cpio_superblock_t *sb = CPIO_SB(parent_dir->superblock);
    cpio_metadata_t metadata;
    const bool found = cpio_read_metadata(sb->dev, path, &metadata);
    if (!found)
        return false; // not found

    cpio_inode_t *i = cpio_inode_create(sb, &metadata);
    dentry->inode = &i->inode;
    return true;
}

static size_t cpio_i_iterate_dir(inode_t *dir, dir_iterator_state_t *state, dentry_iterator_op op)
{
    cpio_inode_t *i = CPIO_INODE(dir);
    cpio_superblock_t *sb = CPIO_SB(dir->superblock);
    blockdev_t *dev = sb->dev;

    char path_prefix[i->metadata.name_length + 1]; // +1 for null terminator
    dev->read(dev, path_prefix, i->metadata.name_length, i->metadata.name_offset);
    path_prefix[i->metadata.name_length] = '\0';
    size_t prefix_len = strlen(path_prefix); // +1 for the slash

    if (strcmp(path_prefix, ".") == 0)
        path_prefix[0] = '\0', prefix_len = 0; // root directory

    const size_t start_nth = state->dir_nth - DIR_ITERATOR_NTH_START; // - 2 because of '.' and '..', so we start at that offset

    // find all children of this directory, that starts with 'path' and doesn't have any more slashes
    size_t written = 0;
    cpio_newc_header_t header;
    size_t offset = 0;
    size_t filtered_n = 0;
    while (true)
    {
        dev->read(dev, &header, sizeof(cpio_newc_header_t), offset);
        offset += sizeof(cpio_newc_header_t);

        if (strncmp(header.magic, "07070", 5) != 0 || (header.magic[5] != '1' && header.magic[5] != '2'))
            mos_panic("invalid cpio header magic, possibly corrupt archive");

        const size_t filename_len = strntoll(header.namesize, NULL, 16, sizeof(header.namesize) / sizeof(char));

        char filename[filename_len + 1]; // +1 for null terminator
        dev->read(dev, filename, filename_len, offset);
        filename[filename_len] = '\0';

        const bool found = strncmp(path_prefix, filename, prefix_len) == 0     // make sure the path starts with the parent path
                           && (prefix_len == 0 || filename[prefix_len] == '/') // make sure it's a child, not a sibling (/path/to vs /path/toooo)
                           && strchr(filename + prefix_len + 1, '/') == NULL;  // make sure it's only one level deep

        const bool is_TRAILER = strcmp(filename, "TRAILER!!!") == 0;
        const bool is_root_dot = strcmp(filename, ".") == 0;

        // only continue after we've found the start_nth file
        if (found && !is_TRAILER && !is_root_dot)
        {
            if (filtered_n++ >= start_nth)
            {
                mos_debug(cpio, "prefix '%s' filename '%s'", path_prefix, filename);

                const u32 modebits = strntoll(header.mode, NULL, 16, sizeof(header.mode) / sizeof(char));
                const file_type_t type = cpio_modebits_to_filetype(modebits & CPIO_MODE_FILE_TYPE);
                const s64 ino = strntoll(header.ino, NULL, 16, sizeof(header.ino) / sizeof(char));

                const char *name = filename + prefix_len + (prefix_len == 0 ? 0 : 1);          // +1 for the slash if it's not the root
                const size_t name_len = filename_len - prefix_len - (prefix_len == 0 ? 0 : 1); // -1 for the slash if it's not the root

                const size_t w = op(state, ino, name, name_len, type);
                written += w;

                // no more space, terminate the iteration
                if (w == 0)
                {
                    mos_debug(cpio, "iteration terminated at %zu, possibly out of space", filtered_n);
                    return written;
                }
            }
        }

        if (unlikely(is_TRAILER))
            break;

        offset += filename_len;
        offset = ((offset + 3) & ~0x03); // align to 4 bytes

        const size_t data_len = strntoll(header.filesize, NULL, 16, sizeof(header.filesize) / sizeof(char));
        offset += data_len;
        offset = ((offset + 3) & ~0x03); // align to 4 bytes (again)
    }

    mos_debug(cpio, "iterated with prefix='%s', started at %zu, wrote %zu bytes for %zu items", path_prefix, start_nth, written, filtered_n);
    return written;
}

static size_t cpio_i_readlink(dentry_t *dentry, char *buffer, size_t buflen)
{
    cpio_metadata_t metadata = CPIO_INODE(dentry->inode)->metadata;
    blockdev_t *dev = CPIO_SB(dentry->inode->superblock)->dev;

    size_t read = dev->read(dev, buffer, MIN(buflen, metadata.data_length), metadata.data_offset);
    return read;
}

static const inode_ops_t cpio_dir_inode_ops = {
    .lookup = cpio_i_lookup,
    .iterate_dir = cpio_i_iterate_dir,
};

static const inode_ops_t cpio_file_inode_ops = {
    .readlink = cpio_i_readlink,
};

static const file_ops_t cpio_file_ops = {
    .open = NULL,
    .read = cpio_f_read,
    .mmap = NULL,
};

filesystem_t fs_cpiofs = {
    .list_node = LIST_HEAD_INIT(fs_cpiofs.list_node),
    .superblocks = LIST_HEAD_INIT(fs_cpiofs.superblocks),
    .name = "cpiofs",
    .mount = cpio_mount,
};
