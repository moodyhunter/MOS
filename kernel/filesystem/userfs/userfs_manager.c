// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/filesystem/userfs/userfs.h"
#include "mos/filesystem/vfs.h"
#include "mos/filesystem/vfs_utils.h"
#include "mos/mm/slab_autoinit.h"
#include "mos/printk.h"
#include "mos/tasks/kthread.h"
#include "proto/filesystem.pb.h"

#include <librpc/rpc.h>
#include <librpc/rpc_server.h>
#include <mos/proto/fs_server.h>
#include <mos_stdio.h>
#include <pb_decode.h>
#include <pb_encode.h>

static slab_t *userfs_slab = NULL;
SLAB_AUTOINIT("userfs", userfs_slab, userfs_t);

RPC_DECL_SERVER_PROTOTYPES(userfs_manager, USERFS_MANAGER_X)

static rpc_result_code_t userfs_manager_register(rpc_server_t *server, mos_rpc_fs_register_request *req, mos_rpc_fs_register_response *resp, void *data)
{
    MOS_UNUSED(server);
    MOS_UNUSED(data);

    userfs_t *userfs = kmalloc(userfs_slab);
    if (!userfs)
        return RPC_RESULT_SERVER_INTERNAL_ERROR;

    size_t userfs_fsnamelen = strlen("userfs.") + strlen(req->fs.name) + 1;
    userfs->fs.name = kmalloc(userfs_fsnamelen);
    if (!userfs->fs.name)
    {
        kfree(userfs);
        return RPC_RESULT_SERVER_INTERNAL_ERROR;
    }

    snprintf((char *) userfs->fs.name, userfs_fsnamelen, "userfs.%s", req->fs.name);
    userfs->rpc_server_name = strdup(req->rpc_server_name);

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

void userfs_manager_rpc_init()
{
    kthread_create(userfs_manager_server_exec, NULL, "fs_rpc_server");
}

MOS_INIT(KTHREAD, userfs_manager_rpc_init);
