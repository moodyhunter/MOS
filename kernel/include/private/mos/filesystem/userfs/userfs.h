// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/filesystem/vfs_types.h"
#include "proto/filesystem.pb.h"

#include <librpc/rpc_client.h>

typedef struct
{
    filesystem_t fs;               ///< The filesystem, "userfs.<name>".
    const char *rpc_server_name;   ///< The name of the RPC server.
    rpc_server_stub_t *rpc_server; ///< The RPC server stub, if connected.
} userfs_t;

/**
 * @brief Ensure that the userfs is connected to the server.
 *
 * @param userfs The userfs to connect.
 */
void userfs_ensure_connected(userfs_t *userfs);

/**
 * @brief Convert a protobuf inode to a kernel inode.
 *
 * @param pbi The protobuf inode.
 * @param sb The superblock.
 * @return inode_t* An allocated kernel inode.
 */
inode_t *i_from_pb(const pb_inode *pbi, superblock_t *sb);

/**
 * @brief Convert a kernel inode to a protobuf inode.
 *
 * @param i The kernel inode.
 * @param pbi The protobuf inode, which must be allocated.
 * @return pb_inode* The protobuf inode, returned for convenience.
 */
pb_inode *i_to_pb(const inode_t *i, pb_inode *pbi);

dentry_t *userfs_fsop_mount(filesystem_t *fs, const char *device, const char *options);
