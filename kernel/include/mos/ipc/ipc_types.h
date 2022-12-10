// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "lib/sync/refcount.h"
#include "mos/io/io.h"
#include "mos/tasks/task_types.h"
#include "mos/types.h"

#define IPC_MSG_MAGIC MOS_FOURCC('I', 'M', 's', 'g')

/**
 * \brief Flags for ipc_channel_open()
 * \details
 * FLAG & EXISTENCE | EXISTING  | NON-EXISTING
 * -----------------+-----------+--------------
 * CREATE_ONLY      | FAIL      | CREATE
 * EXISTING_ONLY    | OPEN      | FAIL
 * neither          | OPEN      | CREATE
 * both (invalid)   | FAIL      | FAIL
 */
typedef enum
{
    IPC_OPEN_CREATE_ONLY = 1 << 0,
    IPC_OPEN_EXISTING_ONLY = 1 << 1,
} ipc_open_flags;

typedef struct ipc_msg
{
    u32 magic;
    u32 type;
    u32 length;
    char data[]; // variable length data
} __packed ipc_msg_t;

typedef struct ipc_channel
{
    const char *name;
    paging_handle_t address_space;
    vmblock_t vmblock;
} ipc_server_t;

typedef struct
{
    io_t io;
    ipc_server_t *server;
} ipc_io_t;
