// SPDX-License-Identifier: GPL-3.0-or-later

#include "blockdev.h"
#include "proto/blockdev.pb.h"

#include <bit>
#include <iomanip>
#include <iostream>
#include <librpc/macro_magic.h>
#include <librpc/rpc.h>
#include <librpc/rpc_client.h>
#include <mos/mos_global.h>
#include <optional>
#include <pb_decode.h>
#include <string>

RPC_CLIENT_DEFINE_SIMPLECALL(blockdev_manager, BLOCKDEV_MANAGER_RPC_X)
RPC_CLIENT_DEFINE_SIMPLECALL(blockdev, BLOCKDEV_SERVER_RPC_X)

const std::optional<std::string> query_server_name(const char *device_name)
{
    rpc_server_stub_t *stub = rpc_client_create(BLOCKDEV_MANAGER_RPC_SERVER_NAME);
    if (!stub)
    {
        std::cerr << "Failed to connect to blockdev manager" << std::endl;
        return std::nullopt;
    }

    mos_rpc_blockdev_opendev_request request{
        .device_name = strdup(device_name),
    };

    mos_rpc_blockdev_opendev_response response;

    const auto result = blockdev_manager_open_device(stub, &request, &response);
    if (result != RPC_RESULT_OK)
    {
        std::cerr << "Failed to open blockdev: error " << result << std::endl;
        return std::nullopt;
    }

    if (!response.result.success)
    {
        if (response.result.error)
            std::cerr << "Failed to open blockdev: " << response.result.error << std::endl;
        else
            std::cerr << "Failed to open blockdev, unknown error." << std::endl;
        return std::nullopt;
    }

    const auto name = std::string(response.server_name);
    pb_release(mos_rpc_blockdev_opendev_request_fields, &request);
    pb_release(mos_rpc_blockdev_opendev_response_fields, &response);
    rpc_client_destroy(stub);

    return name;
}

void do_peek_blocks(const std::string &server_name, off_t start, size_t n_blocks)
{

    rpc_server_stub_t *stub = rpc_client_create(server_name.c_str());
    if (!stub)
    {
        std::cerr << "Failed to connect to blockdev" << std::endl;
        return;
    }

    auto read_req = mos_rpc_blockdev_read_request{
        .n_boffset = static_cast<uint64_t>(start),
        .n_blocks = static_cast<uint32_t>(n_blocks),
    };

    mos_rpc_blockdev_read_response read_resp;

    const auto result = blockdev_read_block(stub, &read_req, &read_resp);
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

    pb_release(mos_rpc_blockdev_read_request_fields, &read_req);
    pb_release(mos_rpc_blockdev_read_response_fields, &read_resp);
    rpc_client_destroy(stub);
}

int main(int argc, char **argv)
{
    MOS_UNUSED(argc);
    MOS_UNUSED(argv);

    if (argc != 4)
    {
        std::cout << "Peek Blocks" << std::endl;
        std::cerr << "Usage: " << argv[0] << " <blockdev> <start> <count>" << std::endl;
        std::cerr << "Example: " << argv[0] << " ramdisk 0 1" << std::endl;
        return 1;
    }

    const auto server_name = query_server_name(argv[1]);
    if (!server_name)
    {
        std::cerr << "Failed to query server name" << std::endl;
        return 1;
    }

    const auto start = std::stoll(argv[2]);
    const auto count = std::stoll(argv[3]);
    do_peek_blocks(*server_name, start, count);
    return 0;
}
