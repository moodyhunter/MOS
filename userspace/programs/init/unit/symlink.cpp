#include "symlink.hpp"

#include <unistd.h>

bool Symlink::Start()
{
    if (GetStatus() == UnitStatus::Finished)
        return true;

    SetStatus(UnitStatus::Starting);
    if (symlink(target.c_str(), linkfile.c_str()) != 0)
    {
        SetStatus(UnitStatus::Failed);
        m_error = strerror(errno);
        return false;
    }

    SetStatus(UnitStatus::Finished);
    return true;
}

bool Symlink::Stop()
{
    if (unlink(linkfile.c_str()) != 0)
    {
        SetStatus(UnitStatus::Failed);
        m_error = strerror(errno);
        return false;
    }

    SetStatus(UnitStatus::Exited);
    return true;
}

bool Symlink::onLoad(const toml::table &data)
{
    const auto link = data["link"], target = data["target"];

    if (!link || !target || !link.is_string() || !target.is_string())
        return false;

    this->linkfile = link.as_string()->get();
    this->target = target.as_string()->get();
    return true;
}
