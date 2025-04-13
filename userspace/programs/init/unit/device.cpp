// SPDX-License-Identifier: GPL-3.0-or-later
#include "unit/device.hpp"

#include "ServiceManager.hpp"

RegisterUnit(device, Device);

Device::Device(const std::string &id, toml::table &table, std::shared_ptr<const Template> template_, const ArgumentMap &args)
    : Unit(id, table, template_, args), //
      driver(GetArg(table, "driver")),  //
      driver_args(GetArrayArg(table, "driver_args"))
{
}

bool Device::Start()
{
    status.Started("plugged");
    ServiceManager->OnUnitStarted(this);
    return true;
}

bool Device::Stop()
{
    status.Inactive();
    ServiceManager->OnUnitStopped(this);
    return true;
}

void Device::onPrint(std::ostream &os) const
{
    os << "  driver: " << driver << std::endl;
    os << "  driver_args: ";
    for (const auto &arg : driver_args)
        os << arg << " ";
    os << std::endl;
}
