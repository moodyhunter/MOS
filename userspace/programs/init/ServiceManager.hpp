// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "units/device.hpp"
#include "units/mount.hpp"
#include "units/path.hpp"
#include "units/service.hpp"
#include "units/symlink.hpp"
#include "units/target.hpp"
#include "units/unit.hpp"

#include <memory>
#include <shared_mutex>
#include <string>
#include <toml++/impl/table.hpp>
#include <vector>

template<typename T>
struct Locked
{
    Locked() = default;
    explicit Locked(T item) : item(item) {};

  public:
    std::pair<T &, std::unique_lock<std::shared_mutex>> BeginWrite()
    {
        return { item, std::unique_lock(lock) };
    }

    std::pair<const T &, std::shared_lock<std::shared_mutex>> BeginRead() const
    {
        return { item, std::shared_lock(lock) };
    }

    T Clone() const
    {
        std::shared_lock<std::shared_mutex> l(lock);
        return item;
    }

  private:
    T item;
    mutable std::shared_mutex lock;
};

class ServiceManagerImpl final
{
    friend Unit;
    friend Service;
    friend Symlink;
    friend Mount;
    friend Path;
    friend Target;
    friend Device;

  public:
    explicit ServiceManagerImpl() = default;

  public:
    bool StartUnit(const std::string &id) const;
    bool StopUnit(const std::string &id) const;

    bool StartDefaultTarget() const;

    void OnProcessExit(pid_t pid, int status);

  private:
    void OnUnitStarted(Unit *unit);
    void OnUnitStopped(Unit *unit);

  private:
    std::vector<std::string> GetStartupOrder(const std::string &id) const;

  private:
    Locked<std::vector<std::thread>> startup_jobs;
};

inline const std::unique_ptr<ServiceManagerImpl> ServiceManager = std::make_unique<ServiceManagerImpl>();
