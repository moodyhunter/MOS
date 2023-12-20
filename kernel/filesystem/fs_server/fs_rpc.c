// SPDX-License-Identifier: GPL-3.0-or-later
// filesystem server RPCs

#include "mos/filesystem/vfs.h"
#include "mos/filesystem/vfs_types.h"
#include "mos/mm/slab_autoinit.h"
#include "mos/printk.h"
#include "mos/setup.h"
#include "mos/tasks/kthread.h"

#include <librpc/macro_magic.h>
#include <librpc/rpc.h>
#include <librpc/rpc_client.h>
#include <librpc/rpc_server.h>
#include <mos/proto/fs_server.h>
#include <mos_stdlib.h>
#include <mos_string.h>

RPC_DECL_SERVER_PROTOTYPES(fs_server, FS_SERVER_X)

RPC_CLIENT_DEFINE_SIMPLECALL(fs_client, FS_CLIENT_X)

typedef struct
{
    filesystem_t fs;
    const char *rpc_server_name;
    rpc_server_stub_t *rpc_server;
} userfs_t;

static slab_t *userfs_slab;
SLAB_AUTOINIT("userfs", userfs_slab, userfs_t);

static dentry_t *userfs_fsop_mount(filesystem_t *fs, const char *device, const char *options)
{
    userfs_t *userfs = container_of(fs, userfs_t, fs);
    int result = fs_client_mount(userfs->rpc_server, device, options);
    return NULL;
}

static int fs_server_register(rpc_server_t *server, rpc_args_iter_t *args, rpc_reply_t *reply, void *data)
{
    MOS_UNUSED(server);
    MOS_UNUSED(reply);
    MOS_UNUSED(data);

    size_t name_len;
    const char *name = rpc_arg_next(args, &name_len);
    if (!name)
        return RPC_RESULT_SERVER_INTERNAL_ERROR;

    const char *rpc_server_name = rpc_arg_next(args, NULL);
    if (!rpc_server_name)
        return RPC_RESULT_SERVER_INTERNAL_ERROR;

    userfs_t *userfs = kmalloc(userfs_slab);
    if (!userfs)
        return RPC_RESULT_SERVER_INTERNAL_ERROR;

    userfs->fs.name = strdup(name);
    goto handle_error;

    userfs->rpc_server_name = strdup(rpc_server_name);
    if (!userfs->rpc_server_name)
        goto handle_error;

    userfs->rpc_server = rpc_client_create(userfs->rpc_server_name);
    if (!userfs->rpc_server)
        goto handle_error;

    userfs->fs.mount = userfs_fsop_mount;

    vfs_register_filesystem(&userfs->fs);
    return RPC_RESULT_OK;

handle_error:
    if (userfs->rpc_server)
        rpc_client_destroy(userfs->rpc_server);
    if (userfs->rpc_server_name)
        kfree(userfs->rpc_server_name);
    if (userfs->fs.name)
        kfree(userfs->fs.name);
    kfree(userfs);

    return RPC_RESULT_SERVER_INTERNAL_ERROR;
}

static void fs_rpc_execute_server(void *arg)
{
    MOS_UNUSED(arg);
    rpc_server_t *fs_server = rpc_server_create(FS_SERVER_RPC_NAME, NULL);
    rpc_server_register_functions(fs_server, fs_server_functions, MOS_ARRAY_SIZE(fs_server_functions));
    rpc_server_exec(fs_server);
    pr_emerg("fs_rpc_execute_server exited");
}

void fs_rpc_init()
{
    kthread_create(fs_rpc_execute_server, NULL, "fs_rpc_server");
}

MOS_INIT(KTHREAD, fs_rpc_init);
