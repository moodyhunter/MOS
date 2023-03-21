// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <mos/mm/ipcshm/ipcshm.h>

void ipcfs_init(void);
void ipcfs_register_server(ipcshm_server_t *server);
void ipcfs_unregister_server(ipcshm_server_t *server);
