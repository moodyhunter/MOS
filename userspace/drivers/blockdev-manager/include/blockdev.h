// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "proto/blockdev.pb.h"

#include <librpc/macro_magic.h>

#define BLOCKDEV_MANAGER_RPC_SERVER_NAME "mos.blockdev-manager"

#define BLOCKDEV_MANAGER_RPC_X(ARGS, PB, arg)                                                                                                                            \
    PB(arg, 1, register_device, REGISTER_DEVICE, mos_rpc_blockdev_register_device_request, mos_rpc_blockdev_register_device_response)                                    \
    PB(arg, 2, register_layer_server, REGISTER_LAYER_SERVER, mos_rpc_blockdev_register_layer_server_request, mos_rpc_blockdev_register_layer_server_response)            \
    PB(arg, 3, read_block, READ_BLOCK, mos_rpc_blockdev_read_block_request, mos_rpc_blockdev_read_block_response)                                                        \
    PB(arg, 4, write_block, WRITE_BLOCK, mos_rpc_blockdev_write_block_request, mos_rpc_blockdev_write_block_response)                                                    \
    PB(arg, 5, open_device, OPEN_DEVICE, mos_rpc_blockdev_open_device_request, mos_rpc_blockdev_open_device_response)

RPC_DECL_CPP_TYPE_NAMESPACE(blockdev, BLOCKDEV_MANAGER_RPC_X);

// a device is a base layer
#define BLOCKDEV_DEVICE_RPC_X(ARGS, PB, arg)                                                                                                                             \
    PB(arg, 1, read_block, READ_BLOCK, mos_rpc_blockdev_read_block_request, mos_rpc_blockdev_read_block_response)                                                        \
    PB(arg, 2, write_block, WRITE_BLOCK, mos_rpc_blockdev_write_block_request, mos_rpc_blockdev_write_block_response)

RPC_DECL_CPP_TYPE_NAMESPACE(blockdev, BLOCKDEV_DEVICE_RPC_X);

#define BLOCKDEV_LAYER_RPC_X(ARGS, PB, arg)                                                                                                                              \
    PB(arg, 1, read_partition_block, READ_PARTITION_BLOCK, mos_rpc_blockdev_read_partition_block_request, mos_rpc_blockdev_read_block_response)                          \
    PB(arg, 2, write_partition_block, WRITE_PARTITION_BLOCK, mos_rpc_blockdev_write_partition_block_request, mos_rpc_blockdev_write_block_response)
