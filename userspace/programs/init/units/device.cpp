// SPDX-License-Identifier: GPL-3.0-or-later
#include "units/device.hpp"

#include "ServiceManager.hpp"
#include "common/ConfigurationManager.hpp"

using namespace std::string_literals;

RegisterUnit(device, Device);

Device::Device(const std::string &id, toml::table &table, std::shared_ptr<const Template> template_, const ArgumentMap &args)
    : Unit(id, table, template_, args),     //
      driver_exec(PopArg(table, "driver")), //
      driver_args(GetArrayArg(table, "driver_args"))
{
    toml::table root;
    root.emplace("description", description + " - Device Driver (auto-generated)");
    {
        toml::table options;
        toml::array exec{ driver_exec };
        for (const auto &arg : driver_args)
            exec.push_back(arg);
        options.emplace("exec", exec);
        root.emplace("options", options);
    }
    {
        toml::table service;
        service.emplace("state-change", "notify"s);
        root.emplace("service", service);
    }

    // Create a service unit for the device driver
    const auto driverId = replace_all(this->id, ".device", ".driver");
    driver = Create(driverId, root);
    ConfigurationManager->AddUnit(driver);
}

bool Device::Start()
{
    status.Starting("plugged");
    const auto worker = [this]
    {
        bool ok = ServiceManager->StartUnit(driver->id);
        if (ok)
        {
            status.Started("working");
            ServiceManager->OnUnitStarted(this);
        }
        else
        {
            status.Failed("driver failed");
        }
    };

    std::thread(worker).detach();
    status.Started("starting driver");
    return true;
}

bool Device::Stop()
{
    status.Stopping();
    if (driver->GetStatus().active)
    {
        if (!driver->Stop())
        {
            std::cerr << "Failed to stop device driver service: " << driver->GetDescription() << std::endl;
            status.Failed("driver service failed to stop");
            return false;
        }
    }

    status.Inactive();
    ServiceManager->OnUnitStopped(this);
    return true;
}

void Device::onPrint(std::ostream &os) const
{
    os << "  driver: " << driver_exec << std::endl;
    os << "  driver_args: ";
    for (const auto &arg : driver_args)
        os << arg << " ";
    os << std::endl;
}
