// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/types.h"

typedef enum
{
    FILE_OPEN_READ = 1 << 0,
    FILE_OPEN_WRITE = 1 << 1,
    FILE_OPEN_EXEC = 1 << 2,
} file_open_mode;

typedef struct
{
    u64 accessed;
    u64 created;
    u64 modified;
} file_stat_time_t;

typedef struct _file_stat
{
    u64 dev;
    u64 ino;
    u64 mode;
    u64 nlink;
    u64 uid;
    u64 gid;
    u64 rdev;
    u64 size;
    u64 blksize;
    u64 blocks;
    file_stat_time_t time;
} file_stat_t;

typedef struct _file
{
    void *data;
} file_t;

file_t *file_open(const char *path, file_open_mode mode);
