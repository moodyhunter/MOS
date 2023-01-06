// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/filesystem/filesystem.h"
#include "mos/filesystem/mount.h"
#include "mos/types.h"

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

static_assert(sizeof(cpio_newc_header_t) == 110, "cpio_newc_header has wrong size");

extern const filesystem_t fs_cpio;
