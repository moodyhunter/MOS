// SPDX-License-Identifier: GPL-3.0-or-later

#include "blockdev.h"
#include "proto/blockdev.pb.h"

#include <iomanip>
#include <iostream>
#include <librpc/macro_magic.h>
#include <librpc/rpc.h>
#include <librpc/rpc_client.h>
#include <memory>
#include <mos/mos_global.h>
#include <optional>
#include <pb_decode.h>
#include <string>

using namespace mos_rpc::blockdev;

RPC_CLIENT_DEFINE_STUB_CLASS(BlockDevManagerServerStub, BLOCKDEV_MANAGER_RPC_X)

std::unique_ptr<BlockDevManagerServerStub> manager = nullptr;

static const std::optional<mos_rpc_blockdev_blockdev> do_open_device(const char *device_name)
{
    open_device::request request{ .device_name = strdup(device_name) };
    open_device::response response;
    const auto result = manager->open_device(&request, &response);
    if (result != RPC_RESULT_OK || !response.result.success)
    {
        std::cerr << "Failed to open blockdev: error " << result << std::endl;
        if (response.result.error)
            std::cerr << "Error: " << response.result.error << std::endl;
        return std::nullopt;
    }

    const auto dev = response.device;
    pb_release(mos_rpc_blockdev_open_device_request_fields, &request);
    pb_release(mos_rpc_blockdev_open_device_response_fields, &response);
    return dev;
}

static void do_peek_blocks(const mos_rpc_blockdev_blockdev &device, size_t start, u32 n_blocks)
{
    auto read_req = read_block::request{
        .device = device,
        .n_boffset = start,
        .n_blocks = n_blocks,
    };

    read_block::response read_resp;

    const auto result = manager->read_block(&read_req, &read_resp);
    if (result != RPC_RESULT_OK)
    {
        std::cerr << "Failed to read block: error " << result << std::endl;
        return;
    }

    if (!read_resp.result.success)
    {
        if (read_resp.result.error)
            std::cerr << "Failed to read block: " << read_resp.result.error << std::endl;
        else
            std::cerr << "Failed to read block, unknown error." << std::endl;
        return;
    }

    std::cout << "Read " << read_resp.data->size << " bytes" << std::endl;
    std::cout << "Data: " << std::endl;
    for (size_t i = 0; i < read_resp.data->size; i++)
    {
        std::cout << std::setw(2) << std::hex << static_cast<int>(read_resp.data->bytes[i]) << " ";
        if ((i + 1) % 32 == 0)
            std::cout << std::endl;
    }

    pb_release(mos_rpc_blockdev_read_block_request_fields, &read_req);
    pb_release(mos_rpc_blockdev_read_block_response_fields, &read_resp);
}

int main(int argc, char **argv)
{
    if (argc != 4)
    {
        std::cout << "Peek Blocks" << std::endl;
        std::cerr << "Usage: " << argv[0] << " <blockdev> <start> <count>" << std::endl;
        std::cerr << "Example: " << argv[0] << " ramdisk 0 1" << std::endl;
        return 1;
    }

    manager = std::make_unique<BlockDevManagerServerStub>(BLOCKDEV_MANAGER_RPC_SERVER_NAME);

    const auto device = do_open_device(argv[1]);
    if (!device)
    {
        std::cerr << "Failed to query server name" << std::endl;
        return 1;
    }

    const auto start = std::stoll(argv[2]);
    const auto count = std::stoll(argv[3]);
    do_peek_blocks(*device, start, count);
    return 0;
}
