// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "proto/blockdev.pb.h"

#define BLOCKDEV_MANAGER_RPC_SERVER_NAME "mos.blockdev-manager"

#define BLOCKDEV_MANAGER_RPC_X(ARGS, PB, arg)                                                                                                                            \
    PB(arg, 1, register_blockdev, REGISTER_BLOCKDEV, mos_rpc_blockdev_register_dev_request, mos_rpc_blockdev_register_dev_response)                                      \
    PB(arg, 2, register_layer, REGISTER_LAYER, mos_rpc_blockdev_register_layer_request, mos_rpc_blockdev_register_layer_response)

#define BLOCKDEV_SERVER_RPC_X(ARGS, PB, arg)                                                                                                                             \
    PB(arg, 1, read_block, READ_BLOCK, mos_rpc_blockdev_read_request, mos_rpc_blockdev_read_response)                                                                    \
    PB(arg, 2, write_block, WRITE_BLOCK, mos_rpc_blockdev_write_request, mos_rpc_blockdev_write_response)
