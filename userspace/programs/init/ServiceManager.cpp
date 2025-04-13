// SPDX-License-Identifier: GPL-3.0-or-later

#include "ServiceManager.hpp"

#include "global.hpp"
#include "logging.hpp"
#include "unit/unit.hpp"

#include <functional>
#include <set>

using namespace std::string_literals;

void ServiceManagerImpl::LoadConfiguration(std::vector<toml::table> &&tables_)
{
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
            if (!value.is_table())
            {
                std::cerr << RED("bad table") << " " << key << std::endl;
                continue;
            }

            for (const auto &[subkey, subvalue] : *value.as_table())
            {
                const auto id = key.data() + "."s + subkey.data();
                if (FindUnitByName(id))
                {
                    std::cerr << "unit " << id << " " RED("already exists") << std::endl;
                    continue;
                }

                const auto data = subvalue.as_table();

                if (!data || !data->is_table() || data->empty() || !data->contains("type"))
                {
                    std::cerr << RED("bad unit, expect table with type") << std::endl;
                    continue;
                }

                if (id.ends_with(TEMPLATE_SUFFIX))
                {
                    const auto [templates, lock] = this->templates.BeginWrite();
                    templates[id] = std::make_shared<Template>(id, *data);
                    Debug << "loaded template " << id << std::endl;
                }
                else if (const auto unit = Unit::CreateNew(id, data); unit)
                {
                    const auto [units, lock] = loaded_units.BeginWrite();
                    units[id] = unit;
                    Debug << "created unit " << id << std::endl;
                }
                else
                {
                    std::cerr << RED("failed to create unit ") << id << std::endl;
                }
            }
        }
    }

    // add unit to part_of unit's depends_on
    const auto [units, lock] = loaded_units.BeginWrite();
    for (const auto &[id, unit] : units)
    {
        for (const auto &part : unit->GetPartOf())
        {
            if (units.contains(part))
                units[part]->AddDependency(unit->id);
            else
                std::cerr << "unit " << unit->id << " is part of non-existent unit " << part << std::endl;
        }
    }
}

void ServiceManagerImpl::OnProcessExit(pid_t pid, int status)
{
    // check if any service has exited
    for (const auto &[id, spid] : service_pid)
    {
        if (spid == -1)
            continue;

        if (pid == spid)
        {
            service_pid[id] = -1;
            const auto unit = std::static_pointer_cast<Service>(FindUnitByName(id));
            unit->OnExited(status);
            return;
        }
    }

    if (pid > 0)
    {
        if (WIFEXITED(status))
            std::cout << "process " << pid << " exited with status " << WEXITSTATUS(status) << std::endl;
        else if (WIFSIGNALED(status))
            std::cout << "process " << pid << " killed by signal " << WTERMSIG(status) << std::endl;
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
        const auto unit = FindUnitByName(id);
        for (const auto &dep_id : unit->GetDependencies())
            visit(dep_id);
        order.push_back(id);
    };

    visit(id);
    return order;
}

std::shared_ptr<Unit> ServiceManagerImpl::FindUnitByName(const std::string &name) const
{
    const auto [units, lock] = loaded_units.BeginRead();
    if (units.contains(name))
        return units.at(name);
    return nullptr;
}

std::shared_ptr<Template> ServiceManagerImpl::FindTemplateByName(const std::string &name) const
{
    const auto [templates, lock] = this->templates.BeginRead();
    if (templates.contains(name))
        return templates.at(name);
    return nullptr;
}

bool ServiceManagerImpl::StartUnit(const std::string &id) const
{
    if (!FindUnitByName(id))
    {
        if (!FindUnitByName(id + ".service"))
        {
            std::cerr << RED("unit not found") << " " << id << std::endl;
            return false;
        }

        if (!StartUnit(id + ".service"))
        {
            std::cerr << RED("unit not found") << " " << id << std::endl;
            return false;
        }
        return true;
    }

    const auto order = GetStartupOrder(id);
    for (const auto &unit_id : order)
    {
        const auto unit = FindUnitByName(unit_id);

        Debug << STARTING() << "Starting " << unit->description << " (" << unit->id << ")" << std::endl;
        if (unit->GetStatus().status == UnitStatus::UnitStarted)
            continue;
        if (!unit->Start())
        {
            std::cerr << FAILED() << " Failed to start " << unit->description << ": " << *unit->GetFailReason() << std::endl;
            return false;
        }

        // timed wait for the unit to start
        for (int i = 0; i < 10; i++)
        {
            if (unit->GetStatus().status == UnitStatus::UnitStarted)
                break;
            std::cout << "Waiting for " << unit->description << " to start..." << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        if (unit->GetStatus().status != UnitStatus::UnitStarted)
        {
            std::cerr << FAILED() << " Failed to start " << unit->description << ": Startup Timeout (status: " << unit->GetStatus().status << ")" << std::endl;
            return false;
        }
    }
    return true;
}

bool ServiceManagerImpl::StopUnit(const std::string &id) const
{
    const auto unit = FindUnitByName(id);
    if (!unit)
    {
        if (!FindUnitByName(id + ".service"))
        {
            std::cerr << RED("unit not found") << " " << id << std::endl;
            return false;
        }

        if (!StopUnit(id + ".service"))
        {
            std::cerr << RED("unit not found") << " " << id << std::endl;
            return false;
        }
        return true;
    }

    // first stop all dependent units that depends on this unit
    const auto [units, lock] = loaded_units.BeginRead();
    for (const auto &[_, u] : units)
    {
        const auto deps = u->GetDependencies();
        if (std::find(deps.begin(), deps.end(), id) != deps.end())
            StopUnit(u->id);
    }

    Debug << STOPPING() << "Stopping " << unit->description << " (" << unit->id << ")" << std::endl;
    if (!unit->GetStatus().active)
        return true;

    if (!unit->Stop())
    {
        std::cerr << FAILED() << " Failed to stop " << unit->description << ": " << *unit->GetFailReason() << std::endl;
        return false;
    }
    return true;
}

std::optional<std::string> ServiceManagerImpl::InstantiateUnit(const std::string &template_id, const ArgumentMap &parameters)
{
    const auto template_ = FindTemplateByName(template_id);
    if (!template_)
    {
        std::cerr << RED("template not found") << " " << template_id << std::endl;
        return std::nullopt;
    }

    const auto result = template_->Instantiate(parameters);
    if (!result)
    {
        std::cerr << RED("failed to instantiate unit") << " " << template_id << std::endl;
        return std::nullopt;
    }

    const auto [units, lock] = loaded_units.BeginWrite();
    const auto [id, unit] = *result;
    if (units.contains(id))
    {
        std::cerr << RED("unit already exists") << " " << id << std::endl;
        return std::nullopt;
    }
    units[id] = unit;
    return id;
}

void ServiceManagerImpl::OnUnitStarted(Unit *unit, pid_t pid)
{
    if (unit->GetType() == UnitType::Target)
        std::cout << OK() << " Reached target " << unit->description << std::endl;
    else if (unit->GetType() == UnitType::Service)
    {
        std::cout << OK() << " Started " << unit->description << std::endl;
        service_pid[unit->id] = pid;
    }
    else
    {
        std::cout << OK() << " Started " << unit->description << std::endl;
    }
}

void ServiceManagerImpl::OnUnitStopped(Unit *unit)
{
    if (unit->GetType() == UnitType::Target)
        std::cout << OK() << " Stopped target " << unit->description << std::endl;
    else if (unit->GetType() == UnitType::Service)
    {
        service_pid[unit->id] = -1;
        std::cout << OK() << " Stopped " << unit->description << std::endl;
    }
    else
    {
        std::cout << OK() << " Stopped " << unit->description << std::endl;
    }
}
