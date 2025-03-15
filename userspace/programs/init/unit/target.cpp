#include "target.hpp"

bool Target::Start()
{
    SetStatus(UnitStatus::Running);
    return true;
}

bool Target::Stop()
{
    SetStatus(UnitStatus::Stopped);
    std::cout << "TODO: Terminate all dependant units" << std::endl;
    return true;
}

bool Target::onLoad(const toml::table &)
{
    return true;
}
