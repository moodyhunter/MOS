#include "path.hpp"

#include <sys/stat.h>

bool Path::do_start()
{
    if (mkdir(path.c_str(), 0755) != 0)
        return false;
    return true;
}

bool Path::do_stop()
{
    std::cout << "stopping path " << id << std::endl;
    return true;
}

bool Path::do_load(const toml::table &data)
{
    this->path = data["path"].value_or("unknown");
    return true;
}

void Path::do_print(std::ostream &os) const
{
    os << "  path: " << this->path << std::endl;
}
