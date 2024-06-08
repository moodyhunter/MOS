// SPDX-License-Identifier: GPL-3.0-or-later

#include "gptdisk.hpp"

#include "blockdev.h"
#include "layer-gpt.hpp"
#include "proto/blockdev.pb.h"
#include "uuid.hpp"

#include <cassert>
#include <iostream>
#include <librpc/rpc.h>
#include <optional>
#include <pb_decode.h>
#include <string>

static std::optional<GPT::Header> gpt_read_header(const void *data)
{
    const auto header = static_cast<const GPT::Header *>(data);

    if (strncmp((const char *) header, "EFI PART", 8) != 0)
    {
        std::cout << " (invalid GPT header)" << std::endl;
        return std::nullopt;
    }

    std::cout << " (GPT signature is valid)" << std::endl;
    std::cout << "  Revision: " << header->revision << std::endl;
    std::cout << "  Header size: " << header->header_size << std::endl;
    std::cout << "  Header CRC32: " << header->header_crc32 << std::endl;
    std::cout << "  Current LBA: " << header->current_lba << std::endl;
    std::cout << "  Backup LBA: " << header->backup_lba << std::endl;
    std::cout << "  First usable LBA: " << header->first_usable_lba << std::endl;
    std::cout << "  Last usable LBA: " << header->last_usable_lba << std::endl;
    std::cout << "  Disk GUID: " << UUID{ header->disk_guid } << std::endl;
    std::cout << "  Partition table LBA: " << header->partition_table_lba << std::endl;
    std::cout << "  Partition count: " << header->partition_count << std::endl;
    std::cout << "  Partition entry size: " << header->partition_entry_size << std::endl;
    std::cout << "  Partition table CRC32: " << header->partition_table_crc32 << std::endl;

    return *header;
}

GPTDisk::GPTDisk(const blockdev_handle handle, const std::string &disk_name) : device_handle(handle), disk_name(disk_name)
{
}

bool GPTDisk::initialise_gpt()
{
    if (ready)
        throw std::runtime_error("GPTDisk already initialised");

    const auto has_header = disk_read_header();
    if (!has_header)
        return false;

    const auto has_partitions = disk_read_partitions();
    if (!has_partitions)
        return false;

    ready = true;
    return true;
}

GPT::PartitionEntry GPTDisk::get_partition(size_t index) const
{
    assert(ready);
    return partitions.at(index);
}

u32 GPTDisk::get_partition_count() const
{
    assert(ready);
    return partitions.size();
}

bool GPTDisk::disk_read_header()
{
    const read_block::request read_request = {
        .device = device_handle,
        .n_boffset = 1,
        .n_blocks = 1,
    };

    read_block::response read_resp;
    const auto result = manager->read_block(&read_request, &read_resp);
    if (result != RPC_RESULT_OK || !read_resp.result.success)
    {
        std::cout << " (failed to read block)" << std::endl;
        if (read_resp.result.error)
            std::cout << "    " << read_resp.result.error << std::endl;
        return {};
    }

    const auto header = gpt_read_header(read_resp.data->bytes);
    pb_release(mos_rpc_blockdev_read_block_response_fields, &read_resp);

    if (header)
        this->header = header.value();

    return header.has_value();
}

bool GPTDisk::disk_read_partitions()
{
    // continue reading the partition table
    const read_block::request read_request{
        .device = device_handle,
        .n_boffset = header.partition_table_lba,
        .n_blocks = header.partition_count * header.partition_entry_size / 512,
    };
    read_block::response read_resp;

    const auto result3 = manager->read_block(&read_request, &read_resp);
    if (result3 != RPC_RESULT_OK || !read_resp.result.success)
    {
        std::cout << " (failed to read partition table)" << std::endl;
        if (read_resp.result.error)
            std::cout << "    " << read_resp.result.error << std::endl;
        return false;
    }

    std::cout << "  Partition table:" << std::endl;
    for (size_t i = 0; i < header.partition_count; i++)
    {
        const auto ptr = (const char *) read_resp.data->bytes + i * header.partition_entry_size;
        const auto entry = (const GPT::PartitionEntry *) ptr;

        if (entry->type_guid[0] == 0 && entry->type_guid[1] == 0)
            continue; // skip empty entries

        std::cout << std::resetiosflags(std::ios::hex);
        std::cout << "   Partition " << i << ":" << std::endl;
        std::cout << "     Type GUID: " << UUID{ entry->type_guid } << std::endl;
        std::cout << "     Partition GUID: " << UUID{ entry->partition_guid } << std::endl;
        std::cout << "     First LBA: " << entry->first_lba << std::endl;
        std::cout << "     Last LBA: " << entry->last_lba << std::endl;
        std::cout << "     Attributes: " << entry->attributes << std::endl;
        // std::cout << "     Name: " << name << std::endl;

        partitions.push_back(*entry);
    }

    pb_release(mos_rpc_blockdev_read_block_response_fields, &read_resp);
    return true;
}

size_t GPTDisk::read_partition_block(size_t partition_index, u64 blockoffset, u8 *buffer, u32 nblocks)
{
    assert(ready);
    const auto &partition = partitions.at(partition_index);

    const read_block::request read_request = {
        .device = device_handle,
        .n_boffset = partition.first_lba + blockoffset,
        .n_blocks = nblocks,
    };

    read_block::response read_resp;
    const auto result = manager->read_block(&read_request, &read_resp);
    if (result != RPC_RESULT_OK || !read_resp.result.success)
    {
        std::cout << " (failed to read partition block)" << std::endl;
        if (read_resp.result.error)
            std::cout << "    " << read_resp.result.error << std::endl;
        return 0;
    }

    const auto datasize = read_resp.data->size;
    memcpy(buffer, read_resp.data->bytes, datasize);
    pb_release(mos_rpc_blockdev_read_block_response_fields, &read_resp);

    return datasize;
}

size_t GPTDisk::write_partition_block(size_t partition_index, u64 blockoffset, const u8 *buffer, u32 nblocks)
{
    MOS_UNUSED(partition_index);
    MOS_UNUSED(blockoffset);
    MOS_UNUSED(buffer);
    MOS_UNUSED(nblocks);
    assert(ready);
    return 0;
}
