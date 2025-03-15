// SPDX-License-Identifier: GPL-3.0-or-later

#include "ServiceManager.hpp"

#include "global.hpp"
#include "unit/mount.hpp"
#include "unit/path.hpp"
#include "unit/service.hpp"
#include "unit/symlink.hpp"
#include "unit/target.hpp"
#include "unit/unit.hpp"

#include <abi-bits/wait.h>
#include <functional>
#include <set>

using namespace std::string_literals;

namespace
{
    std::shared_ptr<Unit> create_unit(const std::string &id, const toml::table *data)
    {
        if (!data || !data->is_table() || data->empty() || !data->contains("type"))
        {
            std::cerr << "bad unit, expect table with type" << std::endl;
            return nullptr;
        }

        const auto type = (*data)["type"];
        if (!type.is_string())
        {
            std::cerr << "bad type, expect string" << std::endl;
            return nullptr;
        }

        const auto type_string = type.as_string()->get();

        std::shared_ptr<Unit> unit;
        if (type_string == "service")
            unit = std::make_shared<Service>(id);
        else if (type_string == "target")
            unit = std::make_shared<Target>(id);
        else if (type_string == "path")
            unit = std::make_shared<Path>(id);
        else if (type_string == "mount")
            unit = std::make_shared<Mount>(id);
        else if (type_string == "symlink")
            unit = std::make_shared<Symlink>(id);
        else
            std::cerr << "unknown type " << type_string << std::endl;

        if (!unit)
        {
            std::cerr << "failed to create unit" << std::endl;
            return nullptr;
        }

        unit->load(*data);
        return unit;
    }
} // namespace

ServiceManagerImpl::ServiceManagerImpl()
{
}

void ServiceManagerImpl::LoadConfiguration(std::vector<toml::table> &&tables_)
{
    std::unique_lock lock(units_mutex);
    auto tables = tables_;
    {
        auto &main = tables.front();
        this->defaultTarget = main["default_target"].value_or("default.target");
        main.erase("default_target");
    }

    for (const auto &table : tables)
    {
        for (const auto &[key, value] : table)
        {
            const auto subtable = value.as_table();
            if (!subtable)
            {
                std::cerr << RED("bad table") << " " << key << std::endl;
                continue;
            }

            for (const auto &[subkey, subvalue] : *subtable)
            {
                const auto unit_id = key.data() + "."s + subkey.data();
                if (units.contains(unit_id))
                {
                    std::cerr << "unit " << unit_id << " " RED("already exists") << std::endl;
                    continue;
                }

                if (auto unit = create_unit(unit_id, subvalue.as_table()); unit)
                    units[unit_id] = std::move(unit);
                else
                    std::cerr << RED("Failed to create unit") << std::endl;
            }
        }
    }

    // add unit to part_of unit's depends_on
    for (const auto &[id, unit] : units)
    {
        for (const auto &part : unit->part_of)
        {
            if (units.contains(part))
                units[part]->depends_on.push_back(id);
            else
                std::cerr << "unit " << id << " is part of non-existent unit " << part << std::endl;
        }
    }
}

void ServiceManagerImpl::OnProcessExit(pid_t pid, int status)
{
    if (pid > 0)
    {
        if (WIFEXITED(status))
            std::cout << "init: process " << pid << " exited with status " << WEXITSTATUS(status) << std::endl;
        else if (WIFSIGNALED(status))
            std::cout << "init: process " << pid << " killed by signal " << WTERMSIG(status) << std::endl;
    }

    // check if any service has exited
    for (const auto &[id, spid] : service_pid)
    {
        if (spid == -1)
            continue;

        if (pid == spid)
        {
            std::cout << "init: service " << id << " exited" << std::endl;
            service_pid[id] = -1;
        }
    }
}

std::vector<std::string> ServiceManagerImpl::GetStartupOrder(const std::string &id) const
{
    std::set<std::string> visited;
    std::vector<std::string> order;

    std::function<void(const std::string &)> visit = [&](const std::string &id)
    {
        if (visited.contains(id))
            return;

        visited.insert(id);
        const auto &unit = units.at(id);
        for (const auto &dep_id : unit->depends_on)
            visit(dep_id);
        order.push_back(id);
    };

    visit(id);
    return order;
}

bool ServiceManagerImpl::StartUnit(const std::string &id) const
{
    const auto order = GetStartupOrder(id);
    for (const auto &unit_id : order)
    {
        const auto unit = units.at(unit_id);

        if (debug)
            std::cout << STARTING() << "Starting " << unit->description << " (" << unit->id << ")" << std::endl;
        if (!unit->Start())
        {
            std::cerr << FAILED() << " Failed to start " << unit->description << ": " << *unit->GetFailReason() << std::endl;
            return false;
        }
        else
        {
            if (unit->GetType() == UnitType::Target)
                std::cout << OK() << " Reached target " << unit->description << std::endl;
            else
                std::cout << OK() << " Started " << unit->description << std::endl;
        }
    }

    return true;
}
void ServiceManagerImpl::OnUnitStarted(Service *service, pid_t pid)
{
    service_pid[service->id] = pid;
}
void ServiceManagerImpl::OnUnitStopped(Service *service)
{
    service_pid[service->id] = -1;
}
