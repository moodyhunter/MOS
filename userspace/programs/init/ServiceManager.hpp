// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "unit/device.hpp"
#include "unit/mount.hpp"
#include "unit/path.hpp"
#include "unit/service.hpp"
#include "unit/symlink.hpp"
#include "unit/target.hpp"
#include "unit/template.hpp"
#include "unit/unit.hpp"

#include <map>
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
    void LoadConfiguration(std::vector<toml::table> &&tables);

    bool StartUnit(const std::string &id) const;
    bool StopUnit(const std::string &id) const;

    std::optional<std::string> InstantiateUnit(const std::string &template_id, const ArgumentMap &parameters);

    bool StartDefaultTarget() const
    {
        return StartUnit(defaultTarget);
    }

    void OnProcessExit(pid_t pid, int status);

    std::vector<std::shared_ptr<Unit>> GetAllUnits() const
    {
        std::vector<std::shared_ptr<Unit>> units;
        const auto [units_, _] = loaded_units.BeginRead();
        for (const auto &[_, unit] : units_)
            units.push_back(unit);
        return units;
    }

    std::vector<std::shared_ptr<Template>> GetAllTemplates() const
    {
        std::vector<std::shared_ptr<Template>> templates_;
        const auto [all_templates, _] = this->templates.BeginRead();
        for (const auto &[id, t] : all_templates)
            templates_.push_back(t);
        return templates_;
    }

  private:
    void OnUnitStarted(Unit *unit, pid_t pid = 0);
    void OnUnitStopped(Unit *unit);

  private:
    std::vector<std::string> GetStartupOrder(const std::string &id) const;
    std::shared_ptr<Unit> FindUnitByName(const std::string &name) const;
    std::shared_ptr<Template> FindTemplateByName(const std::string &name) const;

  private:
    Locked<std::vector<std::thread>> startup_jobs;

  private:
    Locked<std::map<std::string, std::shared_ptr<Template>>> templates;
    Locked<std::map<std::string, std::shared_ptr<Unit>>> loaded_units;
    std::map<std::string, pid_t> service_pid;
    std::string defaultTarget;
};

inline const std::unique_ptr<ServiceManagerImpl> ServiceManager = std::make_unique<ServiceManagerImpl>();
