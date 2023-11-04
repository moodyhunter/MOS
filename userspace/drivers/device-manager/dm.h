// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <libconfig/libconfig.h>
#include <librpc/rpc_server.h>

bool start_load_drivers(const config_t *config);
void dm_run_server(rpc_server_t *server);
bool try_start_driver(u16 vendor, u16 device, u32 location, u64 mmio_base);

extern const config_t *dm_config;
