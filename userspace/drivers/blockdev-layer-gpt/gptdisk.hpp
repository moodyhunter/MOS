// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "proto/blockdev.pb.h"

#include <cstddef>
#include <mos/types.h>
#include <string>
#include <vector>

using blockdev_handle = mos_rpc_blockdev_blockdev;

namespace GPT
{
    struct Header
    {
        u64 signature;
        u32 revision;
        u32 header_size;
        u32 header_crc32;
        u32 reserved;
        u64 current_lba;
        u64 backup_lba;
        u64 first_usable_lba;
        u64 last_usable_lba;
        u8 disk_guid[16];
        u64 partition_table_lba;
        u32 partition_count;
        u32 partition_entry_size;
        u32 partition_table_crc32;
        u8 reserved2[420];
    } __packed;

    struct PartitionEntry
    {
        u8 type_guid[16];
        u8 partition_guid[16];
        u64 first_lba;
        u64 last_lba;
        u64 attributes;
        // char name[0]; // UTF-16LE
    } __packed;
} // namespace GPT

class GPTDisk
{
  public:
    explicit GPTDisk(const blockdev_handle handle, const std::string &disk_name);
    ~GPTDisk() = default;

    bool initialise_gpt();

    u32 get_partition_count() const;
    GPT::PartitionEntry get_partition(size_t index) const;

    size_t read_partition_block(size_t partition_index, u64 blockoffset, u8 *buffer, u32 nblocks);
    size_t write_partition_block(size_t partition_index, u64 blockoffset, const u8 *buffer, u32 nblocks);

    size_t get_block_size() const
    {
        return 512;
    }

    std::string name() const
    {
        return disk_name;
    }

  private:
    bool disk_read_header();
    bool disk_read_partitions();

  private:
    const blockdev_handle device_handle;
    std::string disk_name;

    bool ready = false;
    GPT::Header header;
    std::vector<GPT::PartitionEntry> partitions;
};
