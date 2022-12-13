// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/ipc/ipc_types.h"
#include "mos/tasks/task_types.h"

void ipc_init(void);

io_t *ipc_create(const char *name, size_t max_pending_connections);

io_t *ipc_accept(io_t *server);

io_t *ipc_connect(process_t *owner, const char *name, ipc_connect_flags flags, size_t buffer_size);
