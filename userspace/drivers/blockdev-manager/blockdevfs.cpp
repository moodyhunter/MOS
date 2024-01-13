// SPDX-License-Identifier: GPL-3.0-or-later

#include "autodestroy.hpp"
#include "blockdev_manager.hpp"
#include "proto/filesystem.pb.h"

#include <algorithm>
#include <atomic>
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

static int blockdevfs_mount(rpc_server_t *server, mos_rpc_fs_mount_request *req, mos_rpc_fs_mount_response *resp, void *data)
{
    MOS_UNUSED(server);
    MOS_UNUSED(data);

    if (req->options && strlen(req->options) > 0 && strcmp(req->options, "defaults") != 0)
        printf("blockdevfs: mount option '%s' is not supported\n", req->options);

    if (req->device && strlen(req->device) > 0 && strcmp(req->device, "none") != 0)
        printf("blockdevfs: mount: device name '%s' is not supported\n", req->device);

    if (root)
    {
        resp->result.success = false;
        resp->result.error = strdup("blockdevfs: already mounted");
        return 0;
    }

    root = new blockdevfs_inode();

    pb_inode *i = &resp->root_i;
    i->stat.ino = 1;
    i->stat.type = FILE_TYPE_DIRECTORY;
    i->stat.perm = 0755;
    i->stat.uid = 0;
    i->stat.gid = 0;
    i->stat.size = 0;
    i->stat.accessed = i->stat.modified = i->stat.created = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    i->stat.nlinks = 1; // 1 for the directory itself
    i->stat.sticky = false;
    i->stat.suid = false;
    i->stat.sgid = false;
    i->private_data = (ptr_t) root;

    resp->result.success = true;
    resp->result.error = NULL;

    return 0;
}

static int blockdevfs_readdir(rpc_server_t *server, mos_rpc_fs_readdir_request *req, mos_rpc_fs_readdir_response *resp, void *data)
{
    MOS_UNUSED(server);
    MOS_UNUSED(data);

    if (req->inode.private_data != (ptr_t) root)
    {
        resp->result.success = false;
        resp->result.error = strdup("blockdevfs: invalid inode");
        return 0;
    }

    const size_t count = blockdev_list.size();
    resp->entries_count = count;
    resp->entries = (pb_dirent *) malloc(count * sizeof(pb_dirent));

    int i = 0;
    for (const auto &[id, info] : blockdev_list)
    {
        pb_dirent *e = &resp->entries[i++];
        e->name = strdup(info.name.c_str());
        e->ino = id;
        e->type = FILE_TYPE_BLOCK_DEVICE;
    }

    resp->result.success = true;
    resp->result.error = NULL;
    return 0;
}

static int blockdevfs_lookup(rpc_server_t *server, mos_rpc_fs_lookup_request *req, mos_rpc_fs_lookup_response *resp, void *data)
{
    MOS_UNUSED(server);
    MOS_UNUSED(req);
    MOS_UNUSED(resp);
    MOS_UNUSED(data);

    if (req->inode.private_data != (ptr_t) root)
    {
        resp->result.success = false;
        resp->result.error = strdup("blockdevfs: invalid inode");
        return 0;
    }

    const auto it = std::find_if(blockdev_list.begin(), blockdev_list.end(), [&](const auto &p) { return p.second.name == req->name; });
    if (it == blockdev_list.end())
    {
        resp->result.success = false;
        resp->result.error = strdup("blockdevfs: no such block device");
        return 0;
    }

    const auto &[id, info] = *it;

    pb_inode *i = &resp->inode;
    i->stat.ino = id;
    i->stat.type = FILE_TYPE_BLOCK_DEVICE;
    i->stat.perm = 0660;
    i->stat.uid = 0;
    i->stat.gid = 0;
    i->stat.size = info.num_blocks * info.block_size;
    i->stat.accessed = i->stat.modified = i->stat.created = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    i->stat.nlinks = 1;
    i->stat.sticky = false;
    i->stat.suid = false;
    i->stat.sgid = false;
    i->private_data = 0;

    resp->result.success = true;
    resp->result.error = NULL;
    return 0;
}

static int blockdevfs_readlink(rpc_server_t *server, mos_rpc_fs_readlink_request *req, mos_rpc_fs_readlink_response *resp, void *data)
{
    MOS_UNUSED(server);
    MOS_UNUSED(req);
    MOS_UNUSED(resp);
    MOS_UNUSED(data);

    resp->result.success = false;
    resp->result.error = strdup("blockdevfs: no symlinks expected in blockdevfs");
    return 0;
}

static int blockdevfs_getpage(rpc_server_t *server, mos_rpc_fs_getpage_request *req, mos_rpc_fs_getpage_response *resp, void *data)
{
    MOS_UNUSED(server);
    MOS_UNUSED(req);
    MOS_UNUSED(resp);
    MOS_UNUSED(data);

    resp->result.success = false;
    resp->result.error = strdup("blockdevfs doen't support reading or writing pages");
    return 0;
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
