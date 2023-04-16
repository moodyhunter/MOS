// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "common.h"

#include <librpc/rpc_client.h>
#include <mos/device/dm_types.h>
#include <stdio.h>

RPC_CLIENT_DEFINE_SIMPLECALL(console_simple, CONSOLE_RPCS_X)

rpc_server_stub_t *open_console(void);
__printf(1, 2) void print_to_console(const char *fmt, ...);
