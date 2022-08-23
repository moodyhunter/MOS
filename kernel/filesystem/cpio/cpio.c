// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/filesystem/cpio/cpio.h"

#include "lib/string.h"
#include "mos/filesystem/file.h"
#include "mos/filesystem/filesystem.h"
#include "mos/filesystem/fs_fwd.h"
#include "mos/filesystem/mount.h"
#include "mos/mm/kmalloc.h"
#include "mos/printk.h"

bool cpio_read_file(blockdev_t *dev, const char *target, file_t *file)
{
    if (unlikely(strcmp(target, "TRAILER!!!") == 0))
        mos_panic("what the heck are you doing?");

    size_t offset = 0;
    cpio_newc_header_t header;

    while (true)
    {
        dev->read(dev, &header, sizeof(cpio_newc_header_t), offset);
        if (strncmp(header.magic, "07070", 5) != 0 && (header.magic[5] == '1' || header.magic[5] == '2'))
        {
            mos_warn("invalid cpio header magic, possibly corrupt archive");
            return false;
        }

        offset += sizeof(cpio_newc_header_t);

        size_t filename_len = strntoll(header.namesize, NULL, 16, sizeof(header.namesize) / sizeof(char));

        char filename[filename_len];
        dev->read(dev, filename, filename_len, offset);

        bool found = strncmp(filename, target, filename_len) == 0;

        if (found)
        {
            file->uid.uid = strntoll(header.uid, NULL, 16, sizeof(header.uid) / sizeof(char));
            file->gid.gid = strntoll(header.gid, NULL, 16, sizeof(header.gid) / sizeof(char));

            //  0000777  The lower 9 bits specify read/write/execute permissions for world, group, and user following standard POSIX conventions.

            u32 modebits = strntoll(header.mode, NULL, 16, sizeof(header.mode) / sizeof(char));
            file->permissions.other = (modebits & 0007) >> 0;
            file->permissions.group = (modebits & 0070) >> 3;
            file->permissions.owner = (modebits & 0700) >> 6;

            // 0170000 :: This masks the file type bits.
            // 0140000 :: File type value for sockets.
            // 0120000 :: File type value for symbolic links.  For symbolic links, the link body is stored as file data.
            // 0100000 :: File type value for regular files.
            // 0060000 :: File type value for block special devices.
            // 0040000 :: File type value for directories.
            // 0020000 :: File type value for character special devices.
            // 0010000 :: File type value for named pipes or FIFOs.
            // 0004000 :: SUID bit.
            // 0002000 :: SGID bit.
            // 0001000 :: Sticky bit.

            file->sticky = modebits & 0001000;
            file->sgid = modebits & 0002000;
            file->suid = modebits & 0004000;
            switch (modebits & 0170000)
            {
                case 0140000: file->type = FILE_TYPE_SOCKET; break;
                case 0120000: file->type = FILE_TYPE_SYMLINK; break;
                case 0100000: file->type = FILE_TYPE_FILE; break;
                case 0060000: file->type = FILE_TYPE_BLOCK_DEVICE; break;
                case 0040000: file->type = FILE_TYPE_DIRECTORY; break;
                case 0020000: file->type = FILE_TYPE_CHAR_DEVICE; break;
                case 0010000: file->type = FILE_TYPE_NAMED_PIPE; break;
                default: mos_warn("invalid cpio file mode"); return -1;
            }

            file->fsdata = kmalloc(sizeof(cpio_metadata_t));
            cpio_metadata_t *data = get_fsdata(file, cpio_metadata_t);

            data->header_offset = offset - sizeof(cpio_newc_header_t);
            data->name_offset = offset;
            data->name_length = filename_len;
            data->ino = strntoll(header.ino, NULL, 16, sizeof(header.ino) / sizeof(char));
            data->nlink = strntoll(header.nlink, NULL, 16, sizeof(header.nlink) / sizeof(char));
        }

        if (unlikely(strcmp(filename, "TRAILER!!!") == 0))
            return false;

        offset += filename_len;
        offset = ((offset + 3) & ~0x03); // align to 4 bytes

        size_t data_len = strntoll(header.filesize, NULL, 16, sizeof(header.filesize) / sizeof(char));
        if (found)
        {
            cpio_metadata_t *data = get_fsdata(file, cpio_metadata_t);
            data->data_offset = offset;
            data->data_length = data_len;
        }

        offset += data_len;
        offset = ((offset + 3) & ~0x03); // align to 4 bytes (again)

        if (found)
            return true;
    }

    MOS_UNREACHABLE();
}

bool cpio_mount(blockdev_t *dev, path_t *path)
{
    MOS_UNUSED(path);
    size_t read = 0;
    cpio_newc_header_t header;
    read = dev->read(dev, &header, sizeof(header), 0);
    if (read != sizeof(header))
    {
        mos_warn("failed to read cpio archive");
        return false;
    }

    if (strncmp(header.magic, "07070", 5) != 0 && (header.magic[5] == '1' || header.magic[5] == '2'))
    {
        mos_warn("invalid cpio archive format, only the new ASCII format is supported");
        return false;
    }

    pr_info("cpio header: %.6s", header.magic);

    return true;
}

bool cpio_unmount(mountpoint_t *mountpoint)
{
    MOS_ASSERT(mountpoint->fs == &fs_cpio);
    return true;
}

bool cpio_open(const mountpoint_t *mp, const path_t *path, const char *strpath, file_open_flags flags, file_t *file)
{
    MOS_UNUSED(path);
    pr_info("cpio_open: %s", strpath);

    bool result = cpio_read_file(mp->dev, strpath, file);

    // file not found
    if (!result)
        return false;

    return true;
}

bool cpio_read(file_t *file, void *buf, size_t size, size_t *bytes_read)
{
    return true;
}

bool cpio_close(file_t *file)
{
    return true;
}

bool cpio_stat(const mountpoint_t *m, const path_t *path, const char *strpath, file_stat_t *stat)
{
    return true;
}

bool cpio_readlink(const mountpoint_t *m, const path_t *path, const char *strpath, char *buf, size_t size)
{
    return true;
}

filesystem_t fs_cpio = {
    .name = "cpio",
    .op_mount = cpio_mount,
    .op_unmount = cpio_unmount,

    .op_open = cpio_open,
    .op_read = cpio_read,
    .op_close = cpio_close,
    .op_readlink = cpio_readlink,

    .op_stat = cpio_stat,
};
