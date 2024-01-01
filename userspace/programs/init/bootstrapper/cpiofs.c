// SPDX-License-Identifier: GPL-3.0-or-later

#include "cpiofs.h"

#include "bootstrapper.h"

#include <mos/mos_global.h>
#include <mos/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

size_t read_initrd(void *buf, size_t size, size_t offset)
{
    memcpy(buf, (void *) (MOS_INITRD_BASE + offset), size);
    return size;
}

bool cpio_read_metadata(const char *target, cpio_header_t *header, cpio_metadata_t *metadata)
{
    if (unlikely(strcmp(target, "TRAILER!!!") == 0))
    {
        fputs("what the heck are you doing?", stderr);
        abort();
    }

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
