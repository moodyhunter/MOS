// SPDX-License-Identifier: GPL-3.0-or-later

#include "gptdisk.hpp"
#include "layer-gpt.hpp"

#include <cassert>
#include <filesystem>
#include <iostream>
#include <ranges>
#include <type_traits>
#include <unistd.h>

static std::shared_ptr<GPTDisk> open_gpt_disk(const std::string_view disk_path)
{
    std::cout << "Opening GPT disk '" << disk_path << "'..." << std::endl;
    std::filesystem::path disk_path_fs = disk_path;
    const auto disk_name = disk_path_fs.filename().string();

    open_device::request req = { .device_name = strdup(disk_name.c_str()) };
    open_device::response resp;
    const auto result = manager->open_device(&req, &resp);
    if (result != RPC_RESULT_OK || !resp.result.success)
    {
        std::cerr << "Error: failed to open device" << std::endl;
        return nullptr;
    }

    free(req.device_name);
    return std::make_shared<GPTDisk>(resp.device, disk_name);
}

static void do_gpt_scan()
{
    using dentry = std::filesystem::directory_entry;
    using diter = std::filesystem::directory_iterator;

    std::cout << "Scanning for GPT partitions..." << std::endl;

    const auto cond = [](const dentry &e) { return !e.is_directory() && e.path().filename() != "." && e.path().filename() != ".." && e.is_block_file(); };
    for (const auto &entry : diter("/dev/block/") | std::ranges::views::filter(cond))
    {
        const std::string name = entry.path().filename();
        std::cout << "Checking for '" << name << "'...";

        const auto disk = open_gpt_disk(name);
        if (!disk)
        {
            std::cout << " (failed to open disk)" << std::endl;
            continue;
        }

        if (!disk->initialise_gpt())
        {
            std::cout << " (not a valid GPT disk)" << std::endl;
            continue;
        }
    }
}

static void do_gpt_serve(const std::string_view disk_path)
{
    const auto disk = open_gpt_disk(disk_path);
    if (!disk)
    {
        std::cerr << "Error: failed to open disk" << std::endl;
        return;
    }

    if (!disk->initialise_gpt())
    {
        std::cerr << "Error: not a valid GPT disk" << std::endl;
        return;
    }

    GPTLayerServer server(disk, disk->name() + ".gpt");

    std::cout << "Serving GPT partition layer for device " << disk->name() << std::endl;
    server.run();
}

int main(int argc, char *argv[])
{
    enum working_mode
    {
        scan,
        serve
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
        mode = serve;
        disk_path = argv[1];
    }
    else if (argc == 3 && std::string_view(argv[1]) == "--")
    {
        mode = serve;
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
        case serve:
        {
            // now resolve disk_path to a real path
            if (!disk_path.starts_with("/dev/"))
                disk_path = "/dev/block/" + disk_path;

            std::is_constant_evaluated();

            if (access(disk_path.c_str(), F_OK) != 0)
            {
                std::cerr << "Error: " << disk_path << " does not exist" << std::endl;
                return 1;
            }

            do_gpt_serve(disk_path);
            break;
        }

        case scan: do_gpt_scan(); break;
        default: assert(!"Invalid working mode"); // should never happen
    }

    return 0;
}
