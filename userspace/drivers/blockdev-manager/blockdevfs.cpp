// SPDX-License-Identifier: GPL-3.0-or-later

#include "autodestroy.hpp"
#include "blockdev_manager.hpp"
#include "proto/filesystem.pb.h"

#include <chrono>
#include <iostream>
#include <librpc/rpc.h>
#include <librpc/rpc_client.h>
#include <librpc/rpc_server.h>
#include <mos/filesystem/fs_types.h>
#include <mos/proto/fs_server.h>
#include <mos/syscall/usermode.h>
#include <ostream>
#include <pb_decode.h>
#include <pb_encode.h>
#include <sys/stat.h>

RPC_CLIENT_DEFINE_SIMPLECALL(userfs_manager, USERFS_MANAGER_X)
RPC_DECL_SERVER_PROTOTYPES(blockdevfs, USERFS_IMPL_X)

static rpc_server_t *blockdevfs = NULL;
static const auto blockdevfs_guard = mAutoDestroy(blockdevfs, rpc_server_destroy);

#define BLOCKDEVFS_NAME            "blockdevfs"
#define BLOCKDEVFS_RPC_SERVER_NAME "fs.blockdevfs"

struct blockdevfs_inode
{
    std::string blockdev_name;
};

static blockdevfs_inode *root = NULL;

static rpc_result_code_t blockdevfs_mount(rpc_context_t *, mos_rpc_fs_mount_request *req, mos_rpc_fs_mount_response *resp)
{
    if (req->options && strlen(req->options) > 0 && strcmp(req->options, "defaults") != 0)
        printf("blockdevfs: mount option '%s' is not supported\n", req->options);

    if (req->device && strlen(req->device) > 0 && strcmp(req->device, "none") != 0)
        printf("blockdevfs: mount: device name '%s' is not supported\n", req->device);

    if (root)
    {
        resp->result.success = false;
        resp->result.error = strdup("blockdevfs: already mounted");
        return RPC_RESULT_OK;
    }

    root = new blockdevfs_inode();

    pb_inode_info *const i = &resp->root_info;
    i->ino = 1;
    i->type = FILE_TYPE_DIRECTORY;
    i->perm = 0755;
    i->uid = 0;
    i->gid = 0;
    i->size = 0;
    i->accessed = i->modified = i->created = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    i->nlinks = 1; // 1 for the directory itself
    i->sticky = false;
    i->suid = false;
    i->sgid = false;

    resp->root_ref.data = (ptr_t) root;

    resp->result.success = true;
    resp->result.error = NULL;

    return RPC_RESULT_OK;
}

static rpc_result_code_t blockdevfs_readdir(rpc_context_t *, mos_rpc_fs_readdir_request *req, mos_rpc_fs_readdir_response *resp)
{
    if (req->i_ref.data != (ptr_t) root)
    {
        resp->result.success = false;
        resp->result.error = strdup("blockdevfs: invalid inode");
        return RPC_RESULT_OK;
    }

    const size_t count = devices.size();
    resp->entries_count = count;
    resp->entries = (pb_dirent *) malloc(count * sizeof(pb_dirent));

    int i = 0;
    for (const auto &[name, info] : devices)
    {
        pb_dirent *e = &resp->entries[i++];
        e->name = strdup(name.c_str());
        e->ino = info.ino;
        e->type = FILE_TYPE_BLOCK_DEVICE;
    }

    resp->result.success = true;
    resp->result.error = NULL;
    return RPC_RESULT_OK;
}

static rpc_result_code_t blockdevfs_lookup(rpc_context_t *, mos_rpc_fs_lookup_request *req, mos_rpc_fs_lookup_response *resp)
{
    if (req->i_ref.data != (ptr_t) root)
    {
        resp->result.success = false;
        resp->result.error = strdup("blockdevfs: invalid inode");
        return RPC_RESULT_OK;
    }

    if (!devices.contains(req->name))
    {
        resp->result.success = false;
        resp->result.error = strdup("blockdevfs: no such block device");
        return RPC_RESULT_OK;
    }

    const auto &info = devices[req->name];

    pb_inode_info *i = &resp->i_info;
    i->ino = info.ino;
    i->type = FILE_TYPE_BLOCK_DEVICE;
    i->perm = 0660;
    i->uid = 0;
    i->gid = 0;
    i->size = info.num_blocks * info.block_size;
    i->accessed = i->modified = i->created = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    i->nlinks = 1;
    i->sticky = false;
    i->suid = false;
    i->sgid = false;

    resp->result.success = true;
    resp->result.error = NULL;
    return RPC_RESULT_OK;
}

static rpc_result_code_t blockdevfs_readlink(rpc_context_t *, mos_rpc_fs_readlink_request *req, mos_rpc_fs_readlink_response *resp)
{
    MOS_UNUSED(req);

    resp->result.success = false;
    resp->result.error = strdup("blockdevfs: no symlinks expected in blockdevfs");
    return RPC_RESULT_OK;
}

static rpc_result_code_t blockdevfs_getpage(rpc_context_t *, mos_rpc_fs_getpage_request *req, mos_rpc_fs_getpage_response *resp)
{
    MOS_UNUSED(req);

    resp->result.success = false;
    resp->result.error = strdup("blockdevfs doesn't support reading or writing pages");
    return RPC_RESULT_OK;
}

static void *blockdevfs_worker(void *data)
{
    MOS_UNUSED(data);
    pthread_setname_np(pthread_self(), "blockdevfs.worker");

    std::cout << "blockdevfs: worker thread started" << std::endl;

    rpc_server_exec(blockdevfs);

    std::cout << "blockdevfs: worker thread exiting" << std::endl;
    return NULL;
}

bool register_blockdevfs()
{
    blockdevfs = rpc_server_create(BLOCKDEVFS_RPC_SERVER_NAME, NULL);
    if (!blockdevfs)
    {
        std::cout << "blockdevfs: failed to create blockdevfs server" << std::endl;
        return false;
    }

    rpc_server_register_functions(blockdevfs, blockdevfs_functions, MOS_ARRAY_SIZE(blockdevfs_functions));

    rpc_server_stub_t *userfs_manager = rpc_client_create(USERFS_SERVER_RPC_NAME);
    if (!userfs_manager)
    {
        std::cerr << "blockdevfs: failed to connect to the userfs manager" << std::endl;
        return false;
    }

    auto guard = mAutoDestroy(userfs_manager, rpc_client_destroy);
    MOS_UNUSED(guard);

    mos_rpc_fs_register_request req = mos_rpc_fs_register_request_init_zero;
    req.fs.name = strdup(BLOCKDEVFS_NAME);
    req.rpc_server_name = strdup(BLOCKDEVFS_RPC_SERVER_NAME);

    mos_rpc_fs_register_response resp = mos_rpc_fs_register_response_init_zero;
    const rpc_result_code_t result = userfs_manager_register(userfs_manager, &req, &resp);
    if (result != RPC_RESULT_OK || !resp.result.success)
    {
        std::cout << "blockdevfs: failed to register blockdevfs with filesystem server" << std::endl;
        return false;
    }

    pb_release(mos_rpc_fs_register_request_fields, &req);
    pb_release(mos_rpc_fs_register_response_fields, &resp);

    std::cout << "blockdevfs: registered with filesystem server" << std::endl;

    pthread_t worker;
    if (pthread_create(&worker, NULL, blockdevfs_worker, NULL) != 0)
    {
        std::cout << "blockdevfs: failed to create worker thread" << std::endl;
        return false;
    }

    mkdir("/dev", 0755);
    mkdir("/dev/block", 0755);
    long ok = syscall_vfs_mount("none", "/dev/block", "userfs.blockdevfs", "defaults"); // a blocked syscall
    if (IS_ERR_VALUE(ok))
    {
        std::cerr << "Failed to mount blockdevfs" << std::endl;
        return 1;
    }

    return true;
}
