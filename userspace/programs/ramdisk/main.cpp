// SPDX-License-Identifier: GPL-3.0-or-later

#include "blockdev.h"
#include "ramdisk.hpp"

#include <iostream>
#include <librpc/macro_magic.h>
#include <librpc/rpc_client.h>
#include <librpc/rpc_server++.hpp>
#include <librpc/rpc_server.h>
#include <memory>
#include <mos/mos_global.h>
#include <pb_decode.h>
#include <pb_encode.h>

RPC_CLIENT_DEFINE_STUB_CLASS(BlockManager, BLOCKDEV_MANAGER_RPC_X);
RPC_DECL_SERVER_INTERFACE_CLASS(IRamDiskServer, BLOCKDEV_DEVICE_RPC_X);

class RAMDiskServer
    : public IRamDiskServer
    , public RAMDisk
{
  public:
    explicit RAMDiskServer(const std::string &servername, const size_t nbytes) : IRamDiskServer(servername), RAMDisk(nbytes)
    {
    }

    rpc_result_code_t read_block(rpc_context_t *, mosrpc_blockdev_read_block_request *req, mosrpc_blockdev_read_block_response *resp) override
    {
        if (req->n_boffset + req->n_blocks > nblocks())
        {
            resp->result.success = false;
            resp->result.error = strdup("Out of bounds");
            return RPC_RESULT_OK;
        }

        resp->data = (pb_bytes_array_t *) malloc(PB_BYTES_ARRAY_T_ALLOCSIZE(req->n_blocks * block_size()));
        const auto read = RAMDisk::read_block(req->n_boffset, req->n_blocks, resp->data->bytes);
        resp->data->size = read * block_size();

        resp->result.success = true;
        resp->result.error = nullptr;

        return RPC_RESULT_OK;
    }

    rpc_result_code_t write_block(rpc_context_t *, mosrpc_blockdev_write_block_request *req, mosrpc_blockdev_write_block_response *resp) override
    {
        RAMDisk::write_block(req->n_boffset, req->n_blocks, req->data->bytes);

        resp->result.success = true;
        resp->result.error = nullptr;

        resp->n_blocks = req->n_blocks;

        return RPC_RESULT_OK;
    }
};

int main(int argc, char **argv)
{
    std::cout << "RAMDisk for MOS" << std::endl;

    std::string blockdev_name;
    size_t size = 0;

    const auto get_size = [](const char *str) -> size_t
    {
        size_t size = 0;
        if (sscanf(str, "%zu", &size) != 1)
            std::cerr << "Invalid size" << std::endl, exit(1);

        if (strstr(str, "K"))
            size *= 1 KB;
        else if (strstr(str, "M"))
            size *= 1 MB;
        else if (strstr(str, "G"))
            size *= 1 GB;

        return size;
    };

    switch (argc)
    {
        case 1: blockdev_name = "ramdisk", size = 1 MB; break;
        case 2:
        {
            size = get_size(argv[1]);
            blockdev_name = "ramdisk";
            break;
        }
        case 3:
        {
            size = get_size(argv[1]);
            blockdev_name = argv[2];
            break;
        }
        default:
        {
            std::cerr << "Usage: " << argv[0] << " <size> [name]" << std::endl;
            std::cerr << "       " << argv[0] << " 1024" << std::endl;
            std::cerr << "       " << argv[0] << " 1M my_disk" << std::endl;
            std::cerr << "       " << argv[0] << " 5G my_disk2" << std::endl;
            return 1;
        }
    }

    RAMDiskServer ramdisk_server("ramdisk." + blockdev_name, size);

    const auto blockdev_manager = std::make_unique<BlockManager>(BLOCKDEV_MANAGER_RPC_SERVER_NAME);

    mosrpc_blockdev_register_device_request req{ .server_name = strdup(ramdisk_server.get_name().c_str()),
                                                 .device_info = {
                                                     .name = strdup(blockdev_name.c_str()),
                                                     .size = ramdisk_server.nblocks() * ramdisk_server.block_size(),
                                                     .block_size = ramdisk_server.block_size(),
                                                     .n_blocks = ramdisk_server.nblocks(),
                                                 } };
    mosrpc_blockdev_register_device_response resp;
    blockdev_manager->register_device(&req, &resp);
    if (!resp.result.success)
    {
        if (!resp.result.error)
        {
            std::cerr << "Failed to register blockdev: unknown error" << std::endl;
            return -1;
        }

        std::cerr << "Failed to register blockdev: " << resp.result.error << std::endl;
        return -1;
    }

    // int ret = resp.id;
    ramdisk_server.run();
    std::cout << "RAMDisk server terminated" << std::endl;
    return 0;
}
