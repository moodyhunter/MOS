// SPDX-License-Identifier: GPL-3.0-or-later

#include "blockdev.h"
#include "ext4.h"
#include "ext4_blockdev.h"
#include "ext4_dir.h"
#include "ext4_fs.h"
#include "ext4_types.h"
#include "mos/proto/fs_server.h"
#include "proto/blockdev.pb.h"
#include "proto/filesystem.pb.h"

#include <cassert>
#include <iostream>
#include <librpc/rpc.h>
#include <librpc/rpc_client.h>
#include <librpc/rpc_server++.hpp>
#include <librpc/rpc_server.h>
#include <memory>
#include <pb_decode.h>
#include <string>
#include <string_view>

using namespace mos_rpc::blockdev;

RPC_DECL_SERVER_INTERFACE_CLASS(IExt4Server, USERFS_IMPL_X);
RPC_CLIENT_DEFINE_STUB_CLASS(BlockdevManager, BLOCKDEV_MANAGER_RPC_X);

static struct
{
    std::unique_ptr<BlockdevManager> manager;
    mos_rpc_blockdev_blockdev dev;
} state;

class Ext4UserFS : public IExt4Server
{
  public:
    explicit Ext4UserFS(const std::string &name) : IExt4Server(name){};

  private:
    virtual rpc_result_code_t mount(rpc_context_t *ctx, mos_rpc_fs_mount_request *req, mos_rpc_fs_mount_response *resp) override
    {
        std::cout << "mount" << std::endl;
        return RPC_RESULT_OK;
    }

    virtual rpc_result_code_t readdir(rpc_context_t *ctx, mos_rpc_fs_readdir_request *req, mos_rpc_fs_readdir_response *resp) override
    {
        std::cout << "readdir" << std::endl;
        return RPC_RESULT_OK;
    }

    virtual rpc_result_code_t lookup(rpc_context_t *ctx, mos_rpc_fs_lookup_request *req, mos_rpc_fs_lookup_response *resp) override
    {
        std::cout << "lookup" << std::endl;
        return RPC_RESULT_OK;
    }

    virtual rpc_result_code_t readlink(rpc_context_t *ctx, mos_rpc_fs_readlink_request *req, mos_rpc_fs_readlink_response *resp) override
    {
        std::cout << "readlink" << std::endl;
        return RPC_RESULT_OK;
    }

    virtual rpc_result_code_t getpage(rpc_context_t *ctx, mos_rpc_fs_getpage_request *req, mos_rpc_fs_getpage_response *resp) override
    {
        std::cout << "getpage" << std::endl;
        return RPC_RESULT_OK;
    }
};

Ext4UserFS ext4_userfs("ext4");

static int blockdev_open(struct ext4_blockdev *bdev)
{
    std::cout << "open" << std::endl;
    return 0;
}

static int blockdev_bread(struct ext4_blockdev *bdev, void *buf, uint64_t blk_id, uint32_t blk_cnt)
{
    std::cout << "bread(" << blk_id << ", " << blk_cnt << ")" << std::endl;

    read_block::request req{ .device = state.dev, .n_boffset = blk_id, .n_blocks = blk_cnt };
    read_block::response resp;

    const auto result = state.manager->read_block(&req, &resp);

    if (result != RPC_RESULT_OK || !resp.result.success)
    {
        std::cerr << "Failed to read block" << std::endl;
        if (resp.result.error)
            std::cerr << "Error: " << resp.result.error << std::endl;
        return -1;
    }

    assert(resp.data->size == 512 * blk_cnt); // 512 bytes per block (hardcoded)

    memcpy(buf, resp.data->bytes, resp.data->size);

    pb_release(&mos_rpc_blockdev_read_block_request_msg, &req);
    pb_release(&mos_rpc_blockdev_read_block_response_msg, &resp);

    return 0;
}

static int blockdev_bwrite(struct ext4_blockdev *bdev, const void *buf, uint64_t blk_id, uint32_t blk_cnt)
{
    std::cout << "bwrite(" << blk_id << ", " << blk_cnt << ")" << std::endl;
    return 0;
}

static int blockdev_bclose(struct ext4_blockdev *bdev)
{
    std::cout << "close" << std::endl;
    return 0;
}

static int blockdev_lock(struct ext4_blockdev *bdev)
{
    std::cout << "lock" << std::endl;
    return 0;
}

static int blockdev_unlock(struct ext4_blockdev *bdev)
{
    std::cout << "unlock" << std::endl;
    return 0;
}

static uint8_t blockdev_ph_bbuf[512] = { 0 };

static struct ext4_blockdev_iface blockdev_iface = {
    .open = blockdev_open,
    .bread = blockdev_bread,
    .bwrite = blockdev_bwrite,
    .close = blockdev_bclose,
    .lock = blockdev_lock,
    .unlock = blockdev_unlock,
    .ph_bsize = 512,
    .ph_bcnt = 1024,
    .ph_bbuf = blockdev_ph_bbuf,
};

static struct ext4_blockdev ext4_dev = {
    .bdif = &blockdev_iface,
    .part_offset = 0,
    .part_size = (1024) * (512),
};

bool open_blockdev(const std::string &name)
{

    open_device::request req{ .device_name = strdup(name.c_str()) };
    open_device::response resp;
    const auto result = state.manager->open_device(&req, &resp);

    if (result != RPC_RESULT_OK || !resp.result.success)
    {
        std::cerr << "Failed to open block device" << std::endl;
        if (resp.result.error)
            std::cerr << "Error: " << resp.result.error << std::endl;
        return false;
    }

    state.dev = resp.device;
    pb_release(&mos_rpc_blockdev_open_device_request_msg, &req);
    pb_release(&mos_rpc_blockdev_open_device_response_msg, &resp);

    std::cout << "Block Device '" << name << "' opened" << std::endl;
    return true;
}

int main(int argc, char **argv)
{
    std::cout << "EXT2/3/4 File System Driver for MOS" << std::endl;

    if (argc < 2)
    {
        std::cout << "Usage:" << std::endl;
        std::cout << "ext4 <blockdev>" << std::endl;
        return 1;
    }

    for (int i = 0; i < argc; i++)
        std::cout << "argv[" << i << "] = " << argv[i] << std::endl;

    const std::string device = argv[1];

    state.manager = std::make_unique<BlockdevManager>(BLOCKDEV_MANAGER_RPC_SERVER_NAME);
    if (!open_blockdev(device))
    {
        std::cerr << "Failed to open block device" << std::endl;
        return 1;
    }

    ext4_device_register(&ext4_dev, "dev");

    if (ext4_mount("dev", "/", false))
    {
        std::cerr << "Failed to mount ext4 filesystem" << std::endl;
        return 1;
    }

    const auto fs = ext4_dev.fs;

    ext4_inode_ref root;
    ext4_fs_get_inode_ref(fs, EXT4_ROOT_INO, &root);

    return 0;
}
