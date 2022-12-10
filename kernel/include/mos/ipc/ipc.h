// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/ipc/ipc_types.h"
#include "mos/tasks/task_types.h"

void ipc_init(void);
io_t *ipc_create_server(process_t *owner, vmblock_t vmblock, const char *name);
io_t *ipc_create_client(vmblock_t vmblock, const char *name);

io_t *ipc_open(process_t *owner, vmblock_t vmblock, const char *name, ipc_open_flags flags);
