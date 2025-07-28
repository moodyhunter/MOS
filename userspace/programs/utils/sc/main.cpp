// SPDX-License-Identifier: GPL-3.0-or-later

#include "proto/services.pb.h"
#include "proto/services.service.h"

#include <algorithm>
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
#define C_BLUE   "\033[1;34m"
#define C_GRAY   "\033[1;30m"
#define C_WHITE  "\033[1;37m"
#define C_RESET  "\033[0m"

using namespace std::string_literals;

constexpr auto SERVICE_MANAGER_RPC_NAME = "mos.service_manager";
const auto ServiceManager = std::make_shared<ServiceManagerStub>(SERVICE_MANAGER_RPC_NAME);

struct Command
{
    const std::string_view name;
    const std::string_view description;
    int (*const handler)(int, char *[]);
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

const int UnitNameLength = 35;

static int UpdateMaxUnitNameLength(pb_size_t units_count, RpcUnit *units)
{
    int max = UnitNameLength;
    for (size_t i = 0; i < units_count; i++)
        max = std::max(max, int(strlen(units[i].name)));

    return max + 4;
}

static int do_list(int, char **)
{
    GetUnitsRequest req;
    GetUnitsResponse resp;
    ServiceManager->get_units(&req, &resp);
    int unitNameLen = UpdateMaxUnitNameLength(resp.units_count, resp.units);

    printf(C_WHITE "  %-*s %-10s %-30s %-31s  %-30s\n" C_RESET, unitNameLen, "Unit Name", "Type", "Status", "Since", "Description");
    const std::string line(2 + unitNameLen + 1 + 10 + 1 + 30 + 1 + 31 + 2 + 30, '=');
    puts(line.c_str());

    for (size_t i = 0; i < resp.units_count; i++)
    {
        const auto &unit = resp.units[i];
        const auto color = GetStatusColor(unit);

        std::string statusmsg = GetUnitStatus(unit);
        if (unit.status.isActive)
            statusmsg += std::string(" (") + unit.status.statusMessage + ")";

        std::string ctime = std::ctime(&unit.status.timestamp);
        ctime.erase(ctime.length() - 1); // remove newline

        printf("  %s●" C_RESET " %-*s " C_YELLOW "%-10s" C_RESET " %s%-30s%s %s%-*s%s  %s\n", //
               color,
               unitNameLen - 2,                         //
               unit.name,                               //
               GetUnitType(unit),                       //
               color,                                   //
               statusmsg.c_str(),                       //
               C_RESET,                                 //
               unit.status.isActive ? C_WHITE : C_GRAY, //
               int(ctime.size() + 7),                   //
               ctime.c_str(),                           //
               C_RESET,                                 //
               unit.description                         //
        );

        for (auto j = 0u; j < unit.overridden_units_count; j++)
        {
            const auto &overridden = unit.overridden_units[j];
            printf("   " C_GRAY "%s └─ %s\n", std::string(3 * j, ' ').c_str(), overridden.base_unit_id);
        }
    }

    return 0;
}

static int do_list_templates(int, char **)
{
    GetTemplatesRequest req;
    GetTemplatesResponse resp;
    ServiceManager->get_templates(&req, &resp);

    // print header
    constexpr auto ArgsLength = 25;
    constexpr auto ParamsLength = 15;
    const int n = printf(C_WHITE "  %-*s %-*s%-*s%-*s\n" C_RESET, //
                         UnitNameLength, "Template Name",         //
                         ArgsLength, "Predefined Args",           //
                         ParamsLength, "Parameters",              //
                         60, "Description");

    printf("%s\n", std::string(n, '=').c_str());

    for (size_t i = 0; i < resp.templates_count; i++)
    {
        const auto &template_ = resp.templates[i];
        const auto nlines = std::max(1u, std::max(template_.parameters_count, template_.predefined_arguments_count));

        for (size_t j = 0; j < nlines; j++)
        {
            if (j == 0)
            {
                // print first line
                printf(C_GREEN "  ●" C_RESET " %-*s" C_RESET, UnitNameLength - 1, template_.base_id);
            }
            else
            {
                printf("  %-*s", UnitNameLength + 1, "");
            }

            std::string argument;
            if (j < template_.predefined_arguments_count)
                argument = std::string(C_YELLOW) + template_.predefined_arguments[j].name + C_RESET " = " C_GREEN + template_.predefined_arguments[j].value + C_RESET;
            else if (j == 0)
                argument = "None";

            std::string parameter;
            if (j < template_.parameters_count)
                parameter = std::string(C_YELLOW) + template_.parameters[j] + C_RESET;
            else if (j == 0)
                parameter = "None";

            const int argument_color_length = j < template_.predefined_arguments_count ? strlen(C_YELLOW) + strlen(C_RESET) + strlen(C_GREEN) + strlen(C_RESET) : 0;
            const int parameter_color_length = j < template_.parameters_count ? strlen(C_YELLOW) + strlen(C_RESET) : 0;

            printf(C_YELLOW "%-*s" C_RESET, ArgsLength + argument_color_length, argument.c_str());
            printf(C_YELLOW "%-*s" C_RESET, ParamsLength + parameter_color_length, parameter.c_str());

            if (j == 0)
                printf(C_RESET "%s", template_.description);
            puts("");
        }
        printf(C_RESET "%s\n" C_RESET, std::string(n, '-').c_str());
    }
    return 0;
}

static int do_list_overrides(int, char **)
{
    GetUnitOverridesRequest req;
    GetUnitOverridesResponse resp;
    ServiceManager->get_unit_overrides(&req, &resp);

    // header line
    const auto n = printf(C_WHITE "  %-*s %-*s\n" C_RESET,   //
                          UnitNameLength, "Overridden Unit", //
                          UnitNameLength, "Base Unit & Predefined Args");

    printf("%s\n", std::string(n, '=').c_str());
    for (size_t i = 0; i < resp.overrides_count; i++)
    {
        const auto &override = resp.overrides[i];
        printf(C_GREEN "  ● " C_RESET "%s%-*s%s%s%-*s%s\n",                       //
               C_RESET, UnitNameLength - 1, override.overridden_unit_id, C_RESET, //
               C_WHITE, UnitNameLength, override.base_unit_id, C_RESET            //
        );

        for (size_t j = 0; j < override.overrides_count; j++)
        {
            const auto &param = override.overrides[j];
            printf("    %-*s" C_YELLOW "%s" C_RESET " = " C_GREEN "%s" C_RESET "\n", UnitNameLength - 1, "", param.name, param.value);
        }
        printf(C_RESET "%s\n", std::string(n, '-').c_str());
    }
    return 0;
}

static int do_start_unit(int argc, char **argv)
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

static int do_stop_unit(int argc, char **argv)
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

static int do_instantiate(int argc, char **argv)
{
    if (argc < 1)
    {
        std::cerr << "Usage: sc instantiate <template_id> [param1=value1] [param2=value2] ..." << std::endl;
        return 1;
    }

    InstantiateUnitRequest req;
    req.template_id = argv[0];
    req.parameters_count = argc - 1;
    req.parameters = static_cast<mosrpc_KeyValuePair *>(malloc(argc * sizeof(mosrpc_KeyValuePair)));
    for (int i = 1; i < argc; i++)
    {
        const auto &param = argv[i];
        const auto eq = strchr(param, '=');
        if (!eq)
        {
            std::cerr << "Invalid parameter: " << param << std::endl;
            return 1;
        }

        req.parameters[i - 1].name = strndup(param, eq - param);
        req.parameters[i - 1].value = strdup(eq + 1);
    }

    for (int i = 0; i < argc - 1; i++)
        std::cout << "param " << req.parameters[i].name << " = " << req.parameters[i].value << std::endl;

    InstantiateUnitResponse resp;
    const auto err = ServiceManager->instantiate_unit(&req, &resp);

    if (err != RPC_RESULT_OK)
        std::cerr << "Failed to instantiate unit: error " << err << std::endl;
    else
        std::cout << "Unit instantiated: " << resp.unit_id << std::endl;
    return err;
}

static int do_listall(int, char **)
{
    puts("List of current units:");
    do_list(0, nullptr);
    puts("");
    puts("List of current templates:");
    do_list_templates(0, nullptr);
    puts("");
    puts("List of current unit overrides:");
    do_list_overrides(0, nullptr);
    return 0;
}

static int do_help(int, char *[]);

const std::vector<Command> commands = {
    { "list", "List all services", do_list },
    { "listt", "List all templates", do_list_templates },
    { "listo", "List all unit overrides", do_list_overrides },
    { "listall", "List all services and templates", do_listall },
    { "start", "Start unit", do_start_unit },
    { "stop", "Stop unit", do_stop_unit },
    { "instantiate", "Instantiate unit from template", do_instantiate },
    { "help", "Show help", do_help },
};

static int do_help(int, char **)
{
    std::cout << "Usage: " << program_invocation_name << " <command> [args...]" << std::endl;
    std::cout << "Commands:" << std::endl;
    for (const auto &cmd : commands)
        std::cout << "  " << cmd.name << " - " << cmd.description << std::endl;
    return 0;
}

int main(int argc, char **argv)
{
    if (argc == 1)
    {
        do_list(0, nullptr);
        return 0;
    }

    if (argc == 2 && (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0))
    {
        do_help(argc, argv);
        return 0;
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
