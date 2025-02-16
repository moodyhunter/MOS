// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/filesystem/userfs/userfs.hpp"
#include "mos/filesystem/vfs.hpp"
#include "mos/misc/setup.hpp"
#include "mos/syslog/printk.hpp"
#include "mos/tasks/kthread.hpp"
#include "proto/userfs-manager.services.h"

#include <librpc/rpc.h>
#include <librpc/rpc_server.h>
#include <mos/proto/fs_server.h>
#include <mos_stdio.hpp>
#include <pb_decode.h>
#include <pb_encode.h>

MOS_RPC_USERFS_MANAGER_SERVER(userfs_manager)

static rpc_result_code_t userfs_manager_register_filesystem(rpc_context_t *, mosrpc_userfs_register_request *req, mosrpc_userfs_register_response *resp)
{
    userfs_t *userfs = mos::create<userfs_t>();
    if (!userfs)
        return RPC_RESULT_SERVER_INTERNAL_ERROR;

    linked_list_init(list_node(&userfs->fs));

    userfs->fs.name = mos::string("userfs.") + req->fs.name;
    userfs->rpc_server_name = req->rpc_server_name;

    resp->result.success = true;

    userfs->fs.mount = userfs_fsop_mount;
    vfs_register_filesystem(&userfs->fs);
    return RPC_RESULT_OK;
}

static void userfs_manager_server_exec(void *arg)
{
    MOS_UNUSED(arg);
    rpc_server_t *fs_server = rpc_server_create(USERFS_SERVER_RPC_NAME, NULL);
    rpc_server_register_functions(fs_server, userfs_manager_functions, MOS_ARRAY_SIZE(userfs_manager_functions));
    rpc_server_exec(fs_server);
    pr_emerg("fs_rpc_execute_server exited");
}

static void userfs_manager_rpc_init()
{
    kthread_create(userfs_manager_server_exec, NULL, "fs_rpc_server");
}

MOS_INIT(KTHREAD, userfs_manager_rpc_init);
