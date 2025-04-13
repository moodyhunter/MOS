// SPDX-License-Identifier: GPL-3.0-or-later

#include "proto/services.pb.h"
#include "proto/services.service.h"

#include <iostream>
#include <librpc/rpc.h>
#include <memory>
#include <pb_decode.h>
#include <vector>

#define RED(text)    "\033[1;31m" text "\033[0m"
#define GREEN(text)  "\033[1;32m" text "\033[0m"
#define YELLOW(text) "\033[1;33m" text "\033[0m"
#define GRAY(text)   "\033[1;30m" text "\033[0m"
#define WHITE(text)  "\033[1;37m" text "\033[0m"

#define C_RED    "\033[1;31m"
#define C_GREEN  "\033[1;32m"
#define C_YELLOW "\033[1;33m"
#define C_GRAY   "\033[1;30m"
#define C_WHITE  "\033[1;37m"
#define C_RESET  "\033[0m"

constexpr auto SERVICE_MANAGER_RPC_NAME = "mos.service_manager";
const auto ServiceManager = std::make_shared<ServiceManagerStub>(SERVICE_MANAGER_RPC_NAME);

struct Command
{
    std::string name;
    std::string description;
    int (*handler)(int, char *[]);
};

static const char *GetStatusColor(const struct _RpcUnit &unit)
{
    if (!unit.status.isActive)
        return C_GRAY;
    switch (unit.status.status)
    {
        case RpcUnitStatusEnum_Starting: return C_YELLOW;
        case RpcUnitStatusEnum_Started: return C_GREEN;
        case RpcUnitStatusEnum_Failed: return C_RED;
        case RpcUnitStatusEnum_Stopping: return C_YELLOW;
        case RpcUnitStatusEnum_Stopped: return C_RED;
    }

    __builtin_unreachable();
}

static std::string GetUnitStatus(const struct _RpcUnit &unit)
{
    if (!unit.status.isActive)
        return "inactive";

    switch (unit.status.status)
    {
        case RpcUnitStatusEnum_Starting: return "starting";
        case RpcUnitStatusEnum_Started: return "started";
        case RpcUnitStatusEnum_Failed: return "failed";
        case RpcUnitStatusEnum_Stopping: return "stopping";
        case RpcUnitStatusEnum_Stopped: return "stopped";
    }

    __builtin_unreachable();
}

static const char *GetUnitType(const struct _RpcUnit &unit)
{
    switch (unit.type)
    {
        case RpcUnitType_Service: return "Service"; break;
        case RpcUnitType_Target: return "Target"; break;
        case RpcUnitType_Path: return "Path"; break;
        case RpcUnitType_Mount: return "Mount"; break;
        case RpcUnitType_Symlink: return "SymLink"; break;
        case RpcUnitType_Device: return "Device"; break;
        case RpcUnitType_Timer: return "Timer"; break;
    }

    __builtin_unreachable();
}

int list(int, char **)
{
#define UnitNameLength 40

    printf(C_WHITE "  %-" MOS_STRINGIFY(UnitNameLength) "s %-10s %-30s %-31s  %-30s\n" C_RESET, "Unit Name", "Type", "Status", "Since", "Description");
    std::string line(2 + UnitNameLength + 1 + 10 + 1 + 30 + 1 + 31 + 2 + 30, '-');
    puts(line.c_str());

    GetUnitsRequest req;
    GetUnitsResponse resp;
    ServiceManager->get_units(&req, &resp);

    for (size_t i = 0; i < resp.units_count; i++)
    {
        const auto &unit = resp.units[i];
        const auto color = GetStatusColor(unit);

        auto statusmsg = GetUnitStatus(unit);
        if (unit.status.isActive)
            statusmsg += std::string(" (") + unit.status.statusMessage + ")";

        const auto ctime = std::ctime(&unit.status.timestamp);
        printf("%s●" C_RESET " %-" MOS_STRINGIFY(UnitNameLength) "s " C_YELLOW "%-10s" C_RESET " %s%-30s%s %ssince: %.*s%s  %s\n", //
               color,                                                                                                              //
               unit.name,                                                                                                          //
               GetUnitType(unit),                                                                                                  //
               color,                                                                                                              //
               statusmsg.c_str(),                                                                                                  //
               C_RESET,                                                                                                            //
               unit.status.isActive ? C_WHITE : C_GRAY, int(strlen(ctime) - 1),                                                    //
               ctime,                                                                                                              //
               C_RESET,                                                                                                            //
               unit.description                                                                                                    //
        );
    }

    return 0;
}

int list_templates(int, char **)
{
    GetTemplatesRequest req;
    GetTemplatesResponse resp;
    ServiceManager->get_templates(&req, &resp);

    for (size_t i = 0; i < resp.templates_count; i++)
    {
        const auto &template_ = resp.templates[i];
        printf(C_GREEN "●" C_RESET " %s - %s\n", template_.base_id, template_.description);
        printf("  Parameters:\n");
        for (size_t j = 0; j < template_.parameters_count; j++)
            printf(C_RESET "    - %s\n", template_.parameters[j]);
    }
    return 0;
}

int start_unit(int argc, char **argv)
{
    if (argc != 1)
    {
        std::cerr << "Usage: sc start <unit_id>" << std::endl;
        return 1;
    }

    StartUnitRequest req;
    req.unit_id = argv[0];
    StartUnitResponse resp;
    const auto err = ServiceManager->start_unit(&req, &resp);

    if (err != RPC_RESULT_OK)
    {
        std::cerr << "Failed to start unit: error " << err << std::endl;
        return 1;
    }
    return 0;
}

int stop_unit(int argc, char **argv)
{
    if (argc != 1)
    {
        std::cerr << "Usage: sc stop <unit_id>" << std::endl;
        return 1;
    }

    StopUnitRequest req;
    req.unit_id = argv[0];
    StopUnitResponse resp;
    const auto err = ServiceManager->stop_unit(&req, &resp);

    if (err != RPC_RESULT_OK)
    {
        std::cerr << "Failed to stop unit: error " << err << std::endl;
        return 1;
    }
    return 0;
}

int instantiate(int argc, char **argv)
{
    if (argc < 1)
    {
        std::cerr << "Usage: sc instantiate <template_id> [param1=value1] [param2=value2] ..." << std::endl;
        return 1;
    }

    InstantiateUnitRequest req;
    req.template_id = argv[0];
    req.parameters_count = argc - 1;
    req.parameters = static_cast<KeyValuePair *>(malloc(argc * sizeof(KeyValuePair)));
    for (int i = 1; i < argc; i++)
    {
        const auto &param = argv[i];
        const auto eq = strchr(param, '=');
        if (!eq)
        {
            std::cerr << "Invalid parameter: " << param << std::endl;
            return 1;
        }

        req.parameters[i - 1].key = strndup(param, eq - param);
        req.parameters[i - 1].value = strdup(eq + 1);
    }

    for (int i = 0; i < argc - 1; i++)
        std::cout << "param " << req.parameters[i].key << " = " << req.parameters[i].value << std::endl;

    InstantiateUnitResponse resp;
    const auto err = ServiceManager->instantiate_unit(&req, &resp);

    if (err != RPC_RESULT_OK)
        std::cerr << "Failed to instantiate unit: error " << err << std::endl;
    else
        std::cout << "Unit instantiated: " << resp.unit_id << std::endl;
    return err;
}

const std::vector<Command> commands = {
    { "list", "List all services", list },
    { "listt", "List all templates", list_templates },
    { "listall", "List all services and templates",
      [](int, char *[])
      {
          list(0, nullptr);
          list_templates(0, nullptr);
          return 0;
      } },
    { "start", "Start unit", start_unit },
    { "stop", "Stop unit", stop_unit },
    { "instantiate", "Instantiate unit from template", instantiate },
    { "help", "Show help",
      [](int, char *[])
      {
          std::cout << "Usage: sc <command> [args...]" << std::endl;
          std::cout << "Commands:" << std::endl;
          for (const auto &command : commands)
              std::cout << "  " << command.name << " - " << command.description << std::endl;
          return 0;
      } },
};

int main(int argc, char **argv)
{
    if (argc == 1)
    {
        puts("List of current units:");
        list(argc, argv);
        return 0;
    }

    if (argc == 2 && std::string(argv[1]) == "--help")
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

    return it->handler(argc - 2, argv + 2);
}
