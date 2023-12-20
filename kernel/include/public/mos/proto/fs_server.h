// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <librpc/macro_magic.h>
#include <mos/mos_global.h>

#define FS_SERVER_RPC_NAME "mos.fs-server"

#define FS_SERVER_X(X, xarg)                                                                                                                                             \
    X(xarg, 0, register, REGISTER, "ss", ARG(const char *, name), ARG(const char *, rpc_server_name))                                                                    \
    X(xarg, 1, unregister, UNREGISTER, "s", ARG(const char *, name))

RPC_DEFINE_ENUMS(fs_server, FS_SERVER, FS_SERVER_X)

#define FS_CLIENT_X(X, xarg) X(xarg, 0, mount, MOUNT, "ss", ARG(const char *, device), ARG(const char *, options))\
