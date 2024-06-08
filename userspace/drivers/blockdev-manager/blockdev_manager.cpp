// SPDX-License-Identifier: GPL-3.0-or-later

#include "blockdev_manager.hpp"

#include "proto/blockdev.pb.h"

#include <atomic>
#include <cstdlib>
#include <iostream>
#include <librpc/rpc.h>
#include <librpc/rpc_server++.hpp>
#include <librpc/rpc_server.h>
#include <map>
#include <memory>
#include <mos/types.h>
#include <mutex>
#include <pb.h>
#include <pb_decode.h>
#include <pb_encode.h>
#include <string>

std::map<std::string, BlockInfo> devices;      // blockdev id -> blockdev info
static std::atomic_ulong next_blockdev_id = 2; // 1 is reserved for the root directory

static std::shared_ptr<BlockLayerServer> get_layer_server(const std::string &name)
{
    static std::map<std::string, std::shared_ptr<BlockLayerServer>> layer_servers; // server name -> server
    static std::mutex server_lock;
    std::lock_guard<std::mutex> lock(server_lock);

    if (!layer_servers.contains(name))
        layer_servers[name] = std::make_shared<BlockLayerServer>(name);

    return layer_servers[name];
}

static std::shared_ptr<BlockDeviceServer> get_device_server(const std::string &name)
{
    static std::map<std::string, std::shared_ptr<BlockDeviceServer>> device_servers; // server name -> server
    static std::mutex server_lock;
    std::lock_guard<std::mutex> lock(server_lock);

    if (!device_servers.contains(name))
        device_servers[name] = std::make_shared<BlockDeviceServer>(name);

    return device_servers[name];
}

struct ClientFDTable
{
    fd_t next_fd = 0;
    std::map<int, std::string> fd_to_device;
};

void BlockManager::on_connect(rpc_context_t *ctx)
{
    // allocate a new FD table for this client
    set_data(ctx, new ClientFDTable());
}

void BlockManager::on_disconnect(rpc_context_t *ctx)
{
    auto table = get_data<ClientFDTable>(ctx);
    delete table;
    MOS_UNUSED(ctx);
}

rpc_result_code_t BlockManager::register_layer_server(rpc_context_t *, register_layer_server::request *req, register_layer_server::response *resp)
{
    for (size_t i = 0; i < req->partitions_count; i++)
    {
        const auto part = req->partitions[i];
        const BlockInfo info = {
            .ino = next_blockdev_id++,
            .name = part.name,
            .n_blocks = part.size / 512,
            .block_size = 512,
            .type = BlockInfo::BLOCKDEV_LAYER,
            .info = BlockLayerInfo{ .server_name = req->server_name, .partid = part.partid },
        };

        devices.emplace(part.name, info);
    }

    resp->result.success = true;
    resp->result.error = NULL;
    return RPC_RESULT_OK;
}

rpc_result_code_t BlockManager::register_device(rpc_context_t *, register_device::request *req, register_device::response *resp)
{
    if (devices.contains(req->device_info.name))
    {
        std::cout << "Device " << req->device_info.name << " already registered" << std::endl;
        resp->result.success = false;
        resp->result.error = strdup("Device already registered");
        return RPC_RESULT_OK;
    }

    const BlockInfo info = {
        .ino = next_blockdev_id++,
        .name = req->device_info.name,
        .n_blocks = req->device_info.n_blocks,
        .block_size = req->device_info.block_size,
        .type = BlockInfo::BLOCKDEV_DEVICE,
        .info = BlockDeviceInfo{ .server_name = req->server_name },
    };

    devices.emplace(req->device_info.name, info);

    std::cout << "Registered device " << req->device_info.name << " with " << req->device_info.n_blocks << " blocks of size " << req->device_info.block_size << " bytes"
              << std::endl;

    resp->result.success = true;
    resp->result.error = NULL;
    return RPC_RESULT_OK;
}

rpc_result_code_t BlockManager::open_device(rpc_context_t *ctx, open_device::request *req, open_device::response *resp)
{
    const auto name = req->device_name;
    if (!devices.contains(name))
    {
        std::cout << "Device " << name << " not found" << std::endl;
        resp->result.success = false;
        resp->result.error = strdup("Device not found");
        return RPC_RESULT_OK;
    }

    auto fdtable = get_data<ClientFDTable>(ctx);
    const auto fd = fdtable->next_fd++;
    fdtable->fd_to_device[fd] = name;
    resp->device.devid = fd;
    resp->result.success = true;
    resp->result.error = NULL;
    return RPC_RESULT_OK;
}

rpc_result_code_t BlockManager::read_block(rpc_context_t *ctx, read_block::request *req, read_block::response *resp)
{
    auto fdtable = get_data<ClientFDTable>(ctx);
    if (!fdtable->fd_to_device.contains(req->device.devid))
    {
        std::cout << "Invalid device handle " << req->device.devid << std::endl;
        resp->result.success = false;
        resp->result.error = strdup("Invalid device handle");
        return RPC_RESULT_OK;
    }

    auto &device = devices[fdtable->fd_to_device[req->device.devid]];
    if (req->n_boffset >= device.n_blocks)
    {
        std::cout << "Block offset " << req->n_boffset << " out of range" << std::endl;
        resp->result.success = false;
        resp->result.error = strdup("Block offset out of range");
        return RPC_RESULT_OK;
    }

    switch (device.type)
    {
        case BlockInfo::BLOCKDEV_LAYER:
        {
            const auto info = std::get<BlockLayerInfo>(device.info);
            const auto server = get_layer_server(info.server_name);

            mos_rpc_blockdev_read_partition_block_request part_req = {
                .device = { .devid = (u32) -1 },
                .partition = { .partid = info.partid },
                .n_boffset = req->n_boffset,
                .n_blocks = req->n_blocks,
            };

            return server->read_partition_block(&part_req, resp);
        }

        case BlockInfo::BLOCKDEV_DEVICE:
        {
            const auto server = get_device_server(std::get<BlockDeviceInfo>(device.info).server_name);
            return server->read_block(req, resp);
        }

        default: __builtin_unreachable();
    };
}

rpc_result_code_t BlockManager::write_block(rpc_context_t *ctx, write_block::request *req, write_block::response *resp)
{
    auto fdtable = get_data<ClientFDTable>(ctx);
    MOS_UNUSED(fdtable);
    MOS_UNUSED(req);
    MOS_UNUSED(resp);
    return RPC_RESULT_OK;
}
