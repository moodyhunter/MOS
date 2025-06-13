#include "path.hpp"

#include "ServiceManager.hpp"

#include <sys/stat.h>
#include <unistd.h>

RegisterUnit(path, Path);

Path::Path(const std::string &id, toml::table &table, std::shared_ptr<const Template> template_, const ArgumentMap &args)
    : Unit(id, table, template_, args), //
      path(PopArg(table, "path"))
{
    if (path.empty())
        std::cerr << "path: missing path" << std::endl;
}

bool Path::Start()
{
    status.Starting("creating...");
    const auto err = mkdir(path.c_str(), 0755);
    if (err != 0 && errno != EEXIST)
    {
        status.Failed(strerror(errno));
        return false;
    }

    status.Started("created");
    ServiceManager->OnUnitStarted(this);
    return true;
}

bool Path::Stop()
{
    status.Stopping("removing...");
    if (rmdir(path.c_str()) != 0)
    {
        status.Failed(strerror(errno));
        return false;
    }
    status.Inactive();
    ServiceManager->OnUnitStopped(this);
    return true;
}

void Path::onPrint(std::ostream &os) const
{
    os << "  path: " << this->path << std::endl;
}
