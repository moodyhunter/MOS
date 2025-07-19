// SPDX-License-Identifier: GPL-3.0-or-later

#include "layer-gpt.hpp"

#include "blockdev.h"
#include "gptdisk.hpp"

#include <cstdlib>
#include <memory>

using namespace std::string_literals;

std::unique_ptr<BlockdevManagerStub> manager = nullptr;

GPTLayerServer::GPTLayerServer(std::shared_ptr<GPTDisk> disk, const std::string &servername) : IBlockdevLayerService(servername), disk(disk)
{
    const register_layer_server::request req = {
        .server_name = strdup(servername.c_str()),
        .partitions_count = disk->get_partition_count(),
        .partitions = new mosrpc_blockdev_partition_info[disk->get_partition_count()],
    };

    for (size_t i = 0; i < disk->get_partition_count(); i++)
    {
        const auto devname = disk->name() + ".p" + std::to_string(i); // "virtblk.00:03:00.p1"
        const auto partition = disk->get_partition(i);
        req.partitions[i].name = strdup(devname.c_str());
        req.partitions[i].size = (partition.last_lba - partition.first_lba + 1) * disk->get_block_size();
        req.partitions[i].partid = i;
    }

    register_layer_server::response resp;
    const auto result = manager->register_layer_server(&req, &resp);
    if (result != RPC_RESULT_OK || !resp.result.success)
        throw std::runtime_error("Failed to register GPT layer server"s + (resp.result.error ? ": "s + resp.result.error : ""s));

    free(req.server_name);
    for (size_t i = 0; i < disk->get_partition_count(); i++)
        free(req.partitions[i].name);
    delete[] req.partitions;
}

rpc_result_code_t GPTLayerServer::read_partition_block(rpc_context_t *context, mosrpc_blockdev_read_partition_block_request *req, read_block::response *resp)
{
    MOS_UNUSED(context);
    const auto datasize = req->n_blocks * disk->get_block_size();
    resp->data = (pb_bytes_array_t *) malloc(PB_BYTES_ARRAY_T_ALLOCSIZE(datasize));
    resp->data->size = datasize;
    disk->read_partition_block(req->partition.partid, req->n_boffset, resp->data->bytes, req->n_blocks);

    resp->result.success = true;
    resp->result.error = nullptr;
    return RPC_RESULT_OK;
}

rpc_result_code_t GPTLayerServer::write_partition_block(rpc_context_t *context, mosrpc_blockdev_write_partition_block_request *req, write_block::response *resp)
{
    MOS_UNUSED(context);
    MOS_UNUSED(resp);
    disk->write_partition_block(req->partition.partid, req->n_boffset, req->data->bytes, req->n_blocks);
    return RPC_RESULT_OK;
}
