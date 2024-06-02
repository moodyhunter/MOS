// SPDX-License-Identifier: GPL-3.0-or-later

#include "blockdev.h"
#include "proto/blockdev.pb.h"

#include <abi-bits/access.h>
#include <assert.h>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <librpc/macro_magic.h>
#include <librpc/rpc.h>
#include <librpc/rpc_client.h>
#include <librpc/rpc_server.h>
#include <memory>
#include <mos/types.h>
#include <pb_decode.h>
#include <pb_encode.h>
#include <ranges>
#include <string_view>
#include <unistd.h>

using namespace std::string_literals;

RPC_CLIENT_DEFINE_STUB_CLASS(BlockDevManagerServerStub, BLOCKDEV_MANAGER_RPC_X);

std::unique_ptr<BlockDevManagerServerStub> manager = nullptr;

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

struct UUID
{
    UUID(const u8 full[16]) : full{}
    {
        memcpy(this->full, full, 16);
    }

    friend std::ostream &operator<<(std::ostream &os, const UUID &guid);

  private:
    u8 full[16];
};

std::ostream &operator<<(std::ostream &os, const UUID &guid)
{
    const auto flags = os.flags();
    os << std::uppercase << std::hex;

    for (size_t i : { 3, 2, 1, 0, /**/ 5, 4, /**/ 7, 6, /**/ 8, 9, /**/ 10, 11, 12, 13, 14, 15 }) // little endian
    {
        if (i == 5 || i == 7 || i == 8 || i == 10)
            os << '-';

        os << std::setw(2) << std::setfill('0') << (int) guid.full[i];
    }

    return os << std::resetiosflags(flags);
}

static void do_gpt_scan()
{
    std::cout << "Scanning for GPT partitions..." << std::endl;

    const auto filter = [](const auto &e) { return !e.is_directory() && e.path().filename() != "." && e.path().filename() != ".."; };
    for (const auto &entry : std::filesystem::directory_iterator("/dev/block/") | std::ranges::views::filter(filter))
    {
        const std::string name = entry.path().filename();
        std::cout << "Checking for '" << name << "'...";

        if (!entry.is_block_file())
        {
            std::cout << " (not a block device)" << std::endl;
            continue;
        }

        mos_rpc_blockdev_open_device_request req = { .device_name = strdup(name.c_str()) };
        mos_rpc_blockdev_open_device_response resp;
        const auto result = manager->open_device(&req, &resp);
        if (result != RPC_RESULT_OK || !resp.result.success)
        {
            std::cout << " (failed to open device)" << std::endl;
            continue;
        }

        std::cout << " (opened)" << std::endl;

        free(req.device_name);

        mos_rpc_blockdev_read_block_request req2 = {
            .device = resp.device,
            .n_boffset = 1,
            .n_blocks = 1,
        };
        mos_rpc_blockdev_read_block_response resp2;
        const auto result2 = manager->read_block(&req2, &resp2);
        if (result2 != RPC_RESULT_OK)
        {
            std::cout << " (failed to read block)" << std::endl;
            continue;
        }

        if (!resp2.result.success)
        {
            std::cout << " (failed to read block: " << resp2.result.error << ")" << std::endl;
            continue;
        }

        const auto header = (GPT::Header *) resp2.data->bytes;

        if (header->signature != le64_to_cpu(0x5452415020494645))
        {
            std::cout << " (invalid GPT signature)" << std::endl;
            continue;
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

        // continue reading the partition table
        req2.n_boffset = header->partition_table_lba;
        req2.n_blocks = header->partition_count * header->partition_entry_size / 512;

        const auto result3 = manager->read_block(&req2, &resp2);
        if (result3 != RPC_RESULT_OK || !resp2.result.success)
        {
            std::cout << " (failed to read partition table)" << std::endl;
            if (resp2.result.error)
                std::cout << "    " << resp2.result.error << std::endl;
            continue;
        }

        std::cout << "  Partition table:" << std::endl;

        const size_t partition_table_entry_size = header->partition_entry_size;

        const void *ptr = resp2.data->bytes;

        for (size_t i = 0; i < header->partition_count; i++)
        {
            const auto entry = (const GPT::PartitionEntry *) ptr;
            ptr = (const char *) ptr + partition_table_entry_size;

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
        }

        pb_release(mos_rpc_blockdev_read_block_request_fields, &req2);
        pb_release(mos_rpc_blockdev_read_block_response_fields, &resp2);

        std::cout << "done." << std::endl;
    }
}

static void do_gpt_read(std::string_view disk_path)
{
    std::cout << "Reading GPT partition table from '" << disk_path << "'..." << std::endl;

    std::filesystem::path disk_path_fs = disk_path;
    const auto disk_name = disk_path_fs.filename().string();
}

int main(int argc, char *argv[])
{
    enum working_mode
    {
        scan,
        read
    } mode;

    manager = std::make_unique<BlockDevManagerServerStub>(BLOCKDEV_MANAGER_RPC_SERVER_NAME);
    std::string disk_path;

    if (argc == 2 && std::string_view(argv[1]) == "--scan")
    {
        mode = scan;
        do_gpt_scan();
        return 0;
    }
    else if (argc == 2)
    {
        mode = read;
        disk_path = argv[1];
    }
    else if (argc == 3 && std::string_view(argv[1]) == "--")
    {
        mode = read;
        disk_path = argv[2];
    }
    else
    {
        std::cout << "Usage: " << argv[0] << " [--] <disk>" << std::endl;
        std::cout << "       " << argv[0] << " --scan" << std::endl;
        std::cout << "Example: " << std::endl;
        std::cout << "       " << argv[0] << " /dev/disk1" << std::endl;
        std::cout << "       " << argv[0] << " disk1" << std::endl;
        return 1;
    }

    switch (mode)
    {
        case read:
        {
            // now resolve disk_path to a real path
            if (!disk_path.starts_with("/dev/"))
                disk_path = "/dev/block/" + disk_path;

            if (access(disk_path.c_str(), F_OK) != 0)
            {
                std::cerr << "Error: " << disk_path << " does not exist" << std::endl;
                return 1;
            }

            do_gpt_read(disk_path);
            break;
        }

        case scan: do_gpt_scan(); break;
        default: assert(!"Invalid working mode"); // should never happen
    }

    return 0;
}
