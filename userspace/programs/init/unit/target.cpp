#include "target.hpp"

bool Target::do_start()
{
    return true;
}

bool Target::do_stop()
{
    return true;
}

bool Target::do_load(const toml::table &)
{
    return true;
}
