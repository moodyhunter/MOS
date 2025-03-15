#include "path.hpp"

#include <sys/stat.h>
#include <unistd.h>

bool Path::Start()
{
    SetStatus(UnitStatus::Starting);
    if (mkdir(path.c_str(), 0755) != 0)
    {
        SetStatus(UnitStatus::Failed);
        m_error = strerror(errno);
        return false;
    }

    SetStatus(UnitStatus::Running);
    return true;
}

bool Path::Stop()
{
    SetStatus(UnitStatus::Stopping);
    if (!unlink(path.c_str()))
    {
        SetStatus(UnitStatus::Failed);
        m_error = strerror(errno);
        return false;
    }
    SetStatus(UnitStatus::Stopped);
    return true;
}

bool Path::onLoad(const toml::table &data)
{
    this->path = data["path"].value_or("unknown");
    return true;
}

void Path::onPrint(std::ostream &os) const
{
    os << "  path: " << this->path << std::endl;
}
