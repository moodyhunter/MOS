#include "symlink.hpp"

#include "ServiceManager.hpp"

#include <unistd.h>

RegisterUnit(symlink, Symlink);

Symlink::Symlink(const std::string &id, const toml::table &table, std::shared_ptr<const Template> template_, const ArgumentMap &args)
    : Unit(id, table, template_, args),                  //
      linkfile(ReplaceArgs(table["link"].value_or(""))), //
      target(ReplaceArgs(table["target"].value_or("")))
{
}

bool Symlink::Start()
{
    status.Starting();
    if (symlink(target.c_str(), linkfile.c_str()) != 0)
    {
        status.Failed(strerror(errno));
        return false;
    }

    status.Started("created");
    ServiceManager->OnUnitStarted(this);
    return true;
}

bool Symlink::Stop()
{
    status.Stopping();
    if (unlink(linkfile.c_str()) != 0)
    {
        status.Failed(strerror(errno));
        return false;
    }

    status.Inactive();
    ServiceManager->OnUnitStopped(this);
    return true;
}
