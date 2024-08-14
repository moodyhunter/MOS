// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "proto/blockdev.pb.h"
#include "proto/blockdev.services.h"

#include <librpc/macro_magic.h>

#define BLOCKDEV_MANAGER_RPC_SERVER_NAME "mos.blockdev-manager"

RPC_DECL_CPP_TYPE_NAMESPACE(blockdev, BLOCKDEVMANAGER_SERVICE_X);

// a device is a base layer
#define BLOCKDEV_DEVICE_RPC_X(ARGS, PB, arg)                                                                                                                             \
    PB(arg, 1, read_block, READ_BLOCK, mosrpc_blockdev_read_block_request, mosrpc_blockdev_read_block_response)                                                          \
    PB(arg, 2, write_block, WRITE_BLOCK, mosrpc_blockdev_write_block_request, mosrpc_blockdev_write_block_response)

RPC_DECL_CPP_TYPE_NAMESPACE(blockdev, BLOCKDEV_DEVICE_RPC_X);

#define BLOCKDEV_LAYER_RPC_X(ARGS, PB, arg)                                                                                                                              \
    PB(arg, 1, read_partition_block, READ_PARTITION_BLOCK, mosrpc_blockdev_read_partition_block_request, mosrpc_blockdev_read_block_response)                            \
    PB(arg, 2, write_partition_block, WRITE_PARTITION_BLOCK, mosrpc_blockdev_write_partition_block_request, mosrpc_blockdev_write_block_response)
