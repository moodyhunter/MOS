// SPDX-License-Identifier: GPL-3.0-or-later

#include "ServiceManager.hpp"

#include "common/ConfigurationManager.hpp"
#include "global.hpp"
#include "logging.hpp"
#include "units/unit.hpp"

#include <functional>
#include <set>

using namespace std::string_literals;

void ServiceManagerImpl::OnProcessExit(pid_t pid, int status)
{
    // check if any service has exited
    for (const auto &[id, unit] : ConfigurationManager->GetAllUnits())
    {
        if (unit->GetType() != UnitType::Service)
            continue;
        const auto service = std::dynamic_pointer_cast<Service>(unit);
        if (!service)
            continue;

        const auto mainPid = service->GetMainPid();
        if (mainPid == -1)
            continue;

        if (pid == mainPid)
        {
            service->OnExited(status);
            return; // we found the service that exited
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
        const auto unit = ConfigurationManager->GetUnit(id);
        for (const auto &dep_id : unit->GetDependencies())
            visit(dep_id);
        order.push_back(id);
    };

    visit(id);
    return order;
}

bool ServiceManagerImpl::StartUnit(const std::string &id) const
{
    if (!ConfigurationManager->HasUnit(id))
    {
        if (!ConfigurationManager->HasUnit(id + ".service"))
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
        const auto unit = ConfigurationManager->GetUnit(unit_id);

        Debug << STARTING() << "Starting " << unit->GetDescription() << " (" << unit->id << ")" << std::endl;
        if (unit->GetStatus().status == UnitStatus::UnitStarted)
            continue;
        if (!unit->Start())
        {
            std::cerr << std::endl << FAILED() << " Failed to start " << unit->GetDescription() << ": " << *unit->GetFailReason() << std::endl;
            return false;
        }

        // timed wait for the unit to start
        for (int i = 0; i < 100; i++)
        {
            if (unit->GetStatus().status != UnitStatus::UnitStarting)
                break;
            Debug << STARTING() << "Waiting for " << unit->GetDescription() << " to start, n = " << i << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }

        if (unit->GetStatus().status == UnitStatus::UnitFailed)
        {
            std::cerr << FAILED() << " Failed to start " << unit->GetDescription() << ": " << unit->GetStatus().status << std::endl;
            return false;
        }
    }
    return true;
}

bool ServiceManagerImpl::StopUnit(const std::string &id) const
{
    Debug << "Stopping unit: " << id << std::endl;
    const auto unit = ConfigurationManager->GetUnit(id);
    if (!unit)
    {
        if (!ConfigurationManager->HasUnit(id + ".service"))
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
    const auto units = ConfigurationManager->GetAllUnits();
    for (const auto &[unitid, unit] : units)
    {
        const auto deps = unit->GetDependencies();
        if (std::find(deps.begin(), deps.end(), id) != deps.end())
            StopUnit(unitid);
    }

    Debug << STOPPING() << "Real Stopping " << unit->GetDescription() << " (" << unit->id << ")" << std::endl;
    if (!unit->GetStatus().active)
        return true;

    if (unit->GetStatus().status == UnitStatus::UnitStopping || unit->GetStatus().status == UnitStatus::UnitStopped)
    {
        std::cerr << "Unit " << unit->GetDescription() << " is already stopping or stopped." << std::endl;
        return true;
    }

    if (!unit->Stop())
    {
        std::cerr << FAILED() << " Failed to stop " << unit->GetDescription() << ": " << *unit->GetFailReason() << std::endl;
        return false;
    }
    return true;
}

void ServiceManagerImpl::OnUnitStarted(Unit *unit)
{
    if (unit->GetType() == UnitType::Target)
        std::cout << OK() << " Reached target " << unit->description << std::endl;
    else
        std::cout << OK() << " Started " << unit->description << std::endl;
}

void ServiceManagerImpl::OnUnitStopped(Unit *unit)
{
    if (unit->GetType() == UnitType::Target)
        std::cout << OK() << " Stopped target " << unit->description << std::endl;
    else if (unit->GetType() == UnitType::Service)
        std::cout << OK() << " Stopped " << unit->description << std::endl;
    else
        std::cout << OK() << " Stopped " << unit->description << std::endl;
}

bool ServiceManagerImpl::StartDefaultTarget() const
{
    const auto defaultTarget = ConfigurationManager->GetDefaultTarget();
    return StartUnit(defaultTarget);
}
