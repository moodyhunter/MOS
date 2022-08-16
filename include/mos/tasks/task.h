// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/types.h"

typedef struct
{
    uid_t owner_uid;
    gid_t owner_gid;
} file_t;

#define MAX_FD_COUNT 512

typedef struct process
{
    process_id_t process_id;
    uid_t effective_uid;
    struct process *parent;
    page_dir_t *page_dir;
} process_t;

typedef struct
{
    thread_id_t thread_id;
    process_t *process;
    file_t *file_table[MAX_FD_COUNT];
} thread_t;
