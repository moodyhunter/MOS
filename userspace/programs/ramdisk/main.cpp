// SPDX-License-Identifier: GPL-3.0-or-later

#include "blockdev.h"
#include "ramdisk.hpp"

#include <cstdint>
#include <iostream>
#include <librpc/macro_magic.h>
#include <librpc/rpc_client.h>
#include <librpc/rpc_server.h>
#include <memory>
#include <mos/mos_global.h>
#include <pb_decode.h>
#include <pb_encode.h>
#include <system_error>

RPC_CLIENT_DEFINE_SIMPLECALL(blockdev_server, BLOCKDEV_MANAGER_RPC_X);
RPC_DECL_SERVER_PROTOTYPES(ramdisk_server, BLOCKDEV_SERVER_RPC_X);

static std::unique_ptr<RAMDisk> rd = nullptr;

static rpc_result_code_t ramdisk_server_read_block(rpc_server_t *server, mos_rpc_blockdev_read_request *req, mos_rpc_blockdev_read_response *resp, void *data)
{
    MOS_UNUSED(server);
    MOS_UNUSED(data);

    if (req->n_boffset + req->n_blocks > rd->nblocks())
    {
        resp->result.success = false;
        resp->result.error = strdup("Out of bounds");
        return RPC_RESULT_OK;
    }

    resp->data = (pb_bytes_array_t *) malloc(PB_BYTES_ARRAY_T_ALLOCSIZE(req->n_blocks * rd->block_size()));
    const auto read = rd->read_block(req->n_boffset, req->n_blocks, resp->data->bytes);
    resp->data->size = read * rd->block_size();

    resp->result.success = true;
    resp->result.error = nullptr;

    return RPC_RESULT_OK;
}

static rpc_result_code_t ramdisk_server_write_block(rpc_server_t *server, mos_rpc_blockdev_write_request *req, mos_rpc_blockdev_write_response *resp, void *data)
{
    MOS_UNUSED(server);
    MOS_UNUSED(data);

    rd->write_block(req->n_boffset, req->n_blocks, req->data->bytes);

    resp->result.success = true;
    resp->result.error = nullptr;

    resp->n_blocks = req->n_blocks;

    return RPC_RESULT_OK;
}

static int do_register_blockdev(const char *name, const char *rpcserver, const size_t nblocks, const size_t block_size)
{
    rpc_server_stub_t *const blockdev_server = rpc_client_create(BLOCKDEV_MANAGER_RPC_SERVER_NAME);
    if (!blockdev_server)
    {
        std::cerr << "Failed to connect to blockdev server" << std::endl;
        return 1;
    }

    mos_rpc_blockdev_register_dev_request req = mos_rpc_blockdev_register_dev_request_init_default;
    req.blockdev_name = strdup(name);
    req.server_name = strdup(rpcserver);
    req.num_blocks = nblocks;
    req.block_size = block_size;

    mos_rpc_blockdev_register_dev_response resp = mos_rpc_blockdev_register_dev_response_init_default;

    if (blockdev_server_register_blockdev(blockdev_server, &req, &resp) != RPC_RESULT_OK)
    {
        std::cerr << "Failed to register blockdev" << std::endl;
        return -1;
    }

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

    int ret = resp.id;

    pb_release(mos_rpc_blockdev_register_dev_request_fields, &req);
    pb_release(mos_rpc_blockdev_register_dev_response_fields, &resp);
    rpc_client_destroy(blockdev_server);

    return ret;
}

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

    const std::string rpc_server_name = "ramdisk." + blockdev_name;

    rpc_server_t *const server = rpc_server_create(rpc_server_name.c_str(), nullptr);
    if (!server)
    {
        std::cerr << "Failed to create ramdisk server: " << std::generic_category().message(errno) << std::endl;
        return 1;
    }

    rpc_server_register_functions(server, ramdisk_server_functions, MOS_ARRAY_SIZE(ramdisk_server_functions));

    rd = std::make_unique<RAMDisk>(size);

    if (!rd)
    {
        std::cerr << "Failed to create RAMDisk" << std::endl;
        return 1;
    }

    do_register_blockdev(blockdev_name.c_str(), rpc_server_name.c_str(), rd->nblocks(), rd->block_size());

    rpc_server_exec(server);

    std::cout << "RAMDisk server terminated" << std::endl;

    return 0;
}
