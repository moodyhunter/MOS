// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "unit/mount.hpp"
#include "unit/path.hpp"
#include "unit/service.hpp"
#include "unit/symlink.hpp"
#include "unit/target.hpp"
#include "unit/unit.hpp"

#include <abi-bits/pid_t.h>
#include <map>
#include <memory>
#include <shared_mutex>
#include <string>

class ServiceManagerImpl final
{
    friend Unit;
    friend Service;
    friend Symlink;
    friend Mount;
    friend Path;
    friend Target;

  public:
    explicit ServiceManagerImpl();

  public:
    void LoadConfiguration(std::vector<toml::table> &&tables);

    bool StartUnit(const std::string &id) const;

    bool StartDefaultTarget() const
    {
        return StartUnit(defaultTarget);
    }

    void OnProcessExit(pid_t pid, int status);

    std::vector<std::shared_ptr<Unit>> GetAllUnits() const
    {
        std::vector<std::shared_ptr<Unit>> ret;
        {
            std::shared_lock lock(units_mutex);
            for (const auto &[id, unit] : units)
                ret.push_back(unit);
        }
        return ret;
    }

  private:
    void OnUnitStarted(Service *service, pid_t pid);
    void OnUnitStopped(Service *service);

  private:
    std::vector<std::string> GetStartupOrder(const std::string &id) const;

  private:
    bool debug = false;
    std::map<std::string, pid_t> service_pid;

    mutable std::shared_mutex units_mutex;
    std::map<std::string, std::shared_ptr<Unit>> units;

    std::string defaultTarget;
};

inline const std::unique_ptr<ServiceManagerImpl> ServiceManager = std::make_unique<ServiceManagerImpl>();
