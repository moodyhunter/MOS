// SPDX-License-Identifier: GPL-3.0-or-later

#include "blockdevfs.hpp"

#include "blockdev_manager.hpp"
#include "proto/filesystem.pb.h"

#include <assert.h>
#include <chrono>
#include <iostream>
#include <librpc/macro_magic.h>
#include <librpc/rpc.h>
#include <librpc/rpc_client.h>
#include <librpc/rpc_server.h>
#include <memory>
#include <mos/filesystem/fs_types.h>
#include <mos/proto/fs_server.h>
#include <mos/syscall/usermode.h>
#include <ostream>
#include <sys/stat.h>

using namespace std::chrono;

#define BLOCKDEVFS_NAME            "blockdevfs"
#define BLOCKDEVFS_RPC_SERVER_NAME "fs.blockdevfs"

std::unique_ptr<IUserFSServer> blockdevfs;

struct blockdevfs_inode
{
    std::string blockdev_name;
};

static blockdevfs_inode *root = NULL;

rpc_result_code_t BlockdevFSServer::mount(rpc_context_t *, mosrpc_fs_mount_request *req, mosrpc_fs_mount_response *resp)
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

    mosrpc_fs_inode_info *const i = &resp->root_info;
    i->ino = 1;
    i->type = FILE_TYPE_DIRECTORY;
    i->perm = 0755;
    i->uid = i->gid = 0;
    i->size = 0;
    i->accessed = i->modified = i->created = duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
    i->nlinks = 1; // 1 for the directory itself
    i->sticky = false;
    i->suid = false;
    i->sgid = false;

    resp->root_ref.data = (ptr_t) root;

    resp->result.success = true;
    resp->result.error = NULL;

    return RPC_RESULT_OK;
}

rpc_result_code_t BlockdevFSServer::readdir(rpc_context_t *, mosrpc_fs_readdir_request *req, mosrpc_fs_readdir_response *resp)
{
    if (req->i_ref.data != (ptr_t) root)
    {
        resp->result.success = false;
        resp->result.error = strdup("blockdevfs: invalid inode");
        return RPC_RESULT_OK;
    }

    const size_t count = devices.size();
    resp->entries_count = count;
    resp->entries = (mosrpc_fs_pb_dirent *) malloc(count * sizeof(mosrpc_fs_pb_dirent));

    int i = 0;
    for (const auto &[name, info] : devices)
    {
        mosrpc_fs_pb_dirent *e = &resp->entries[i++];
        e->name = strdup(name.c_str());
        e->ino = info.ino;
        e->type = FILE_TYPE_BLOCK_DEVICE;
    }

    resp->result.success = true;
    resp->result.error = NULL;
    return RPC_RESULT_OK;
}

rpc_result_code_t BlockdevFSServer::lookup(rpc_context_t *, mosrpc_fs_lookup_request *req, mosrpc_fs_lookup_response *resp)
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

    mosrpc_fs_inode_info *i = &resp->i_info;
    i->ino = info.ino;
    i->type = FILE_TYPE_BLOCK_DEVICE;
    i->perm = 0660;
    i->uid = 0;
    i->gid = 0;
    i->size = info.n_blocks * info.block_size;
    i->accessed = i->modified = i->created = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    i->nlinks = 1;
    i->sticky = false;
    i->suid = false;
    i->sgid = false;

    resp->result.success = true;
    resp->result.error = NULL;
    return RPC_RESULT_OK;
}

static void *blockdevfs_worker(void *data)
{
    MOS_UNUSED(data);
    pthread_setname_np(pthread_self(), "blockdevfs.worker");
    assert(blockdevfs != nullptr);
    blockdevfs->run();
    std::cout << "blockdevfs: worker thread exiting" << std::endl;
    return NULL;
}

bool register_blockdevfs()
{
    blockdevfs = std::make_unique<BlockdevFSServer>(BLOCKDEVFS_RPC_SERVER_NAME);

    UserfsManager userfs_manager{ USERFS_SERVER_RPC_NAME };
    mosrpc_fs_register_request req = { .fs = { .name = strdup(BLOCKDEVFS_NAME) }, .rpc_server_name = strdup(BLOCKDEVFS_RPC_SERVER_NAME) };
    mosrpc_fs_register_response resp;

    const rpc_result_code_t result = userfs_manager.register_fs(&req, &resp);
    if (result != RPC_RESULT_OK || !resp.result.success)
    {
        std::cout << "blockdevfs: failed to register blockdevfs with filesystem server" << std::endl;
        std::cout << "blockdevfs: " << resp.result.error << std::endl;
        return false;
    }

    pb_release(mosrpc_fs_register_request_fields, &req);
    pb_release(mosrpc_fs_register_response_fields, &resp);

    pthread_t worker;
    if (pthread_create(&worker, NULL, blockdevfs_worker, NULL) != 0)
    {
        std::cout << "blockdevfs: failed to create worker thread" << std::endl;
        return false;
    }

    mkdir("/dev/block", 0755);
    long ok = syscall_vfs_mount("none", "/dev/block", "userfs.blockdevfs", "defaults"); // a blocked syscall
    if (IS_ERR_VALUE(ok))
    {
        std::cerr << "Failed to mount blockdevfs" << std::endl;
        return 1;
    }

    return true;
}
