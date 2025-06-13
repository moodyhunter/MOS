#include "target.hpp"

#include "ServiceManager.hpp"

RegisterUnit(target, Target);

Target::Target(const std::string &id, toml::table &table, std::shared_ptr<const Template> template_, const ArgumentMap &args) //
    : Unit(id, table, template_, args)
{
}

bool Target::Start()
{
    for (const auto &partOf : GetMembers())
    {
        if (!ServiceManager->StartUnit(partOf))
        {
            std::cerr << "Failed to stop unit " << partOf << " while stopping target " << id << std::endl;
        }
    }

    status.Started("reached");
    ServiceManager->OnUnitStarted(this);
    return true;
}

bool Target::Stop()
{
    status.Stopping();
    for (const auto &partOf : GetMembers())
    {
        if (!ServiceManager->StopUnit(partOf))
        {
            std::cerr << "Failed to stop unit " << partOf << " while stopping target " << id << std::endl;
        }
    }

    std::cout << "Target " << id << " stopped." << std::endl;
    status.Inactive();
    ServiceManager->OnUnitStopped(this);
    return true;
}
