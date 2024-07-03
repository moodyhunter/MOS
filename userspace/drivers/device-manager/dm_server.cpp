// SPDX-License-Identifier: GPL-3.0-or-later

#include "dm_server.hpp"

#include "dm_common.hpp"

#include <librpc/macro_magic.h>
#include <librpc/rpc_server++.hpp>
#include <librpc/rpc_server.h>

rpc_result_code_t DeviceManagerServer::register_device(rpc_context_t *context, s32 vendor, s32 devid, s32 location, s64 mmio_base)
{
    MOS_UNUSED(context);
    rpc_result_code_t result = RPC_RESULT_OK;
    try_start_driver(vendor, devid, location, mmio_base);
    return result;
}

rpc_result_code_t DeviceManagerServer::register_driver(rpc_context_t *context)
{
    MOS_UNUSED(context);
    rpc_result_code_t result = RPC_RESULT_OK;
    return result;
}
