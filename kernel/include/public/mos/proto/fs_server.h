// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <librpc/macro_magic.h>
#include <mos/mos_global.h>

#define FS_SERVER_RPC_NAME "mos.fs-server"

#define FS_MANAGER_X(ARGS, PB, xarg)                                                                                                                                     \
    PB(xarg, 0, register, REGISTER, mos_rpc_fs_register_request, mos_rpc_fs_register_response)                                                                           \
    ARGS(xarg, 1, unregister, UNREGISTER, "b", ARG(void *, pb_mos_rpc_fs_unregister_request))

RPC_DEFINE_ENUMS(fs_manager, FS_MANAGER, FS_MANAGER_X)

#define FS_IMPL_X(ARGS, PB, xarg)                                                                                                                                        \
    PB(xarg, 0, mount, MOUNT, mos_rpc_fs_mount_request, mos_rpc_fs_mount_response)                                                                                       \
    PB(xarg, 1, readdir, READDIR, mos_rpc_fs_readdir_request, mos_rpc_fs_readdir_response)                                                                               \
    PB(xarg, 2, lookup, LOOKUP, mos_rpc_fs_lookup_request, mos_rpc_fs_lookup_response)                                                                                   \
    PB(xarg, 3, readlink, READLINK, mos_rpc_fs_readlink_request, mos_rpc_fs_readlink_response)                                                                           \
    PB(xarg, 4, getpage, GETPAGE, mos_rpc_fs_getpage_request, mos_rpc_fs_getpage_response)
