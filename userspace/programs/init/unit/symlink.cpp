#include "symlink.hpp"

#include <unistd.h>

bool Symlink::do_start()
{
    if (symlink(target.c_str(), linkfile.c_str()) != 0)
    {
        m_error = strerror(errno);
        return false;
    }
    return true;
}

bool Symlink::do_stop()
{
    if (unlink(linkfile.c_str()) != 0)
        return false;
    return true;
}

bool Symlink::do_load(const toml::table &data)
{
    const auto link = data["link"], target = data["target"];

    if (!link || !target || !link.is_string() || !target.is_string())
        return false;

    this->linkfile = link.as_string()->get();
    this->target = target.as_string()->get();
    return true;
}
