#include "target.hpp"

#include "ServiceManager.hpp"

RegisterUnit(target, Target);

Target::Target(const std::string &id, const toml::table &table, std::shared_ptr<const Template> template_, const ArgumentMap &args) : Unit(id, table, template_, args)
{
}

bool Target::Start()
{
    status.Started("reached");
    ServiceManager->OnUnitStarted(this);
    return true;
}

bool Target::Stop()
{
    std::cout << "TODO: Terminate all dependant units" << std::endl;
    status.Inactive();
    ServiceManager->OnUnitStopped(this);
    return true;
}
