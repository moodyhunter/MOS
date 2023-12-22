// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/filesystem/vfs_types.h"
#include "proto/filesystem.pb.h"

#include <librpc/rpc_client.h>

typedef struct
{
    filesystem_t fs;
    const char *rpc_server_name;
    rpc_server_stub_t *rpc_server;
} userfs_t;

void userfs_ensure_connected(userfs_t *userfs);

inode_t *i_from_pb(const pb_inode *pbi, superblock_t *sb);
pb_inode *i_to_pb(const inode_t *i, pb_inode *pbi);

dentry_t *userfs_fsop_mount(filesystem_t *fs, const char *device, const char *options);
