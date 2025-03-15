// SPDX-License-Identifier: GPL-3.0-or-later

#include "proto/services.pb.h"
#include "proto/services.service.h"

#include <functional>
#include <iostream>
#include <memory>

#define RED(text)    "\033[1;31m" text "\033[0m"
#define GREEN(text)  "\033[1;32m" text "\033[0m"
#define YELLOW(text) "\033[1;33m" text "\033[0m"

constexpr auto SERVICE_MANAGER_RPC_NAME = "mos.service_manager";
const auto ServiceManager = std::make_shared<ServiceManagerStub>(SERVICE_MANAGER_RPC_NAME);

struct Command
{
    std::string name;
    std::string description;
    std::function<void(int, char **)> handler;
};

static std::string_view GetUnitStatus(const struct _RpcUnit &unit)
{
    switch (unit.status)
    {
        case RpcUnitStatus_Stopped: return YELLOW("Stopped "); break;
        case RpcUnitStatus_Running: return GREEN("Running "); break;
        case RpcUnitStatus_Failed: return RED("Failed  "); break;
        case RpcUnitStatus_Finished: return GREEN("Finished"); break;
        default: return "Unknown?"; break;
    }
}

static std::string_view GetUnitType(const struct _RpcUnit &unit)
{
    switch (unit.type)
    {
        case RpcUnitType_Service: return "Service"; break;
        case RpcUnitType_Target: return "Target"; break;
        case RpcUnitType_Path: return "Path"; break;
        case RpcUnitType_Mount: return "Mount"; break;
        case RpcUnitType_Symlink: return "Symlink"; break;
        case RpcUnitType_Device: return "Device"; break;
        case RpcUnitType_Timer: return "Timer"; break;
        default: return "Unknown"; break;
    }
}

void list(int, char **)
{
    GetUnitsRequest req;
    GetUnitsResponse resp;
    ServiceManager->get_units(&req, &resp);

    for (size_t i = 0; i < resp.units_count; i++)
    {
        const auto &unit = resp.units[i];
        std::cout << GetUnitStatus(unit) << std::flush;
        printf("  %-10s", GetUnitType(unit).data());
        printf("%-25s - ", unit.name);
        std::cout << unit.description << std::endl;
    }
}

const std::vector<Command> commands = {
    { "list", "List all services", list },
};

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        std::cout << "Usage: " << argv[0] << " <command> [args...]" << std::endl;
        std::cout << "Commands:" << std::endl;
        for (const auto &command : commands)
            std::cout << "  " << command.name << " - " << command.description << std::endl;

        return 1;
    }

    const auto command = argv[1];
    const auto it = std::find_if(commands.begin(), commands.end(), [&](const auto &cmd) { return cmd.name == command; });
    if (it == commands.end())
    {
        std::cout << "Unknown command: " << command << std::endl;
        return 1;
    }

    it->handler(argc - 2, argv + 2);
}
