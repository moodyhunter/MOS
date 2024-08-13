// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#define USERFS_SERVER_RPC_NAME "mos.userfs-manager"

#define USERFS_MANAGER_X(ARGS, PB, xarg)                                                                                                                                 \
    /**/                                                                                                                                                                 \
    PB(xarg, 0, register_fs, REGISTER_FS, mosrpc_fs_register_request, mosrpc_fs_register_response)

#define USERFS_IMPL_X(ARGS, PB, xarg)                                                                                                                                    \
    PB(xarg, 0, mount, MOUNT, mosrpc_fs_mount_request, mosrpc_fs_mount_response)                                                                                         \
    PB(xarg, 1, readdir, READDIR, mosrpc_fs_readdir_request, mosrpc_fs_readdir_response)                                                                                 \
    PB(xarg, 2, lookup, LOOKUP, mosrpc_fs_lookup_request, mosrpc_fs_lookup_response)                                                                                     \
    PB(xarg, 3, readlink, READLINK, mosrpc_fs_readlink_request, mosrpc_fs_readlink_response)                                                                             \
    PB(xarg, 4, getpage, GETPAGE, mosrpc_fs_getpage_request, mosrpc_fs_getpage_response)                                                                                 \
    PB(xarg, 5, putpage, PUTPAGE, mosrpc_fs_putpage_request, mosrpc_fs_putpage_response)                                                                                 \
    PB(xarg, 6, create_file, CREATE_FILE, mosrpc_fs_create_file_request, mosrpc_fs_create_file_response)                                                                 \
    PB(xarg, 7, sync_inode, SYNC_INODE, mosrpc_fs_sync_inode_request, mosrpc_fs_sync_inode_response)                                                                     \
    PB(xarg, 8, unlink, UNLINK, mosrpc_fs_unlink_request, mosrpc_fs_unlink_response)\
