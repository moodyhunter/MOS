// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "lib/memory.h"
#include "libuserspace.h"
#include "mos/syscall/usermode.h"
#include "mos/types.h"

typedef struct
{
    u64 size;
    char data[];
} ipc_msg_t;

ipc_msg_t *ipc_msg_create(size_t size);
void ipc_msg_destroy(ipc_msg_t *buffer);

ipc_msg_t *ipc_read_msg(fd_t fd);
bool ipc_write_msg(fd_t fd, ipc_msg_t *buffer);

size_t ipc_read_as_msg(fd_t fd, char *buffer, size_t buffer_size);
bool ipc_write_as_msg(fd_t fd, const char *data, size_t size);
