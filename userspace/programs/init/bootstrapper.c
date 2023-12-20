// SPDX-License-Identifier: GPL-3.0-or-later
// A stage 1 init program for the MOS kernel.
//
// This program is responsible for:
// - Mounting the initrd
// - Starting the device manager, filesystem server
// - Starting the stage 2 init program

#include <fcntl.h>
#include <mos/filesystem/fs_types.h>
#include <mos/mos_global.h>
#include <mos/moslib_global.h>
#include <mos_stdio.h>
#include <mos_stdlib.h>
#include <mos_string.h>

#if !MOS_CONFIG(MOS_MAP_INITRD_TO_INIT)
#error "MOS_MAP_INITRD_TO_INIT must be enabled to use bootstrapper"
#endif

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
} cpio_header_t;

MOS_STATIC_ASSERT(sizeof(cpio_header_t) == 110, "cpio_newc_header has wrong size");

typedef struct
{
    size_t header_offset;
    size_t name_offset;
    size_t name_length;
    size_t data_offset;
    size_t data_length;
} cpio_metadata_t;

static size_t read_initrd(void *buf, size_t size, size_t offset)
{
    memcpy(buf, (void *) (MOS_INITRD_BASE + offset), size);
    return size;
}

static bool cpio_read_metadata(const char *target, cpio_header_t *header, cpio_metadata_t *metadata)
{
    if (unlikely(strcmp(target, "TRAILER!!!") == 0))
        fatal_abort("what the heck are you doing?\n");

    size_t offset = 0;

    while (true)
    {
        read_initrd(header, sizeof(cpio_header_t), offset);

        if (strncmp(header->magic, "07070", 5) != 0 || (header->magic[5] != '1' && header->magic[5] != '2'))
        {
            fprintf(stderr, "WARN: invalid cpio header magic, possibly corrupt archive\n");
            return false;
        }

        offset += sizeof(cpio_header_t);

        const size_t filename_len = strntoll(header->namesize, NULL, 16, sizeof(header->namesize) / sizeof(char));

        char filename[filename_len + 1]; // +1 for null terminator
        read_initrd(filename, filename_len, offset);
        filename[filename_len] = '\0';

        const bool found = strncmp(filename, target, filename_len) == 0;

        if (found)
        {
            metadata->header_offset = offset - sizeof(cpio_header_t);
            metadata->name_offset = offset;
            metadata->name_length = filename_len;
        }

        if (unlikely(strcmp(filename, "TRAILER!!!") == 0))
            return false;

        offset += filename_len;
        offset = ALIGN_UP(offset, 4);

        const size_t data_len = strntoll(header->filesize, NULL, 16, sizeof(header->filesize) / sizeof(char));
        if (found)
        {
            metadata->data_offset = offset;
            metadata->data_length = data_len;
        }

        offset += data_len;
        offset = ALIGN_UP(offset, 4);

        if (found)
            return true;
    }

    unreachable();
}

int main(int argc, const char *argv[])
{
    MOS_UNUSED(argc);
    return syscall_execveat(FD_CWD, "/initrd/programs/init", argv, NULL, 0);
}
