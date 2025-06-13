// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "ServiceManager.hpp"
#include "units/template.hpp"

#include <memory>
#include <toml++/impl/table.hpp>
#include <toml++/toml.hpp>
#include <vector>

class ConfigurationManagerImpl
{
  public:
    ConfigurationManagerImpl() = default;
    ~ConfigurationManagerImpl() = default;

    void LoadConfiguration(std::vector<toml::table> &&tables);
    void FinaliseConfiguration();

    inline std::string GetDefaultTarget() const
    {
        return defaultTarget;
    }

    bool HasUnit(const std::string &id) const
    {
        const auto [units, lock] = units_.BeginRead();
        return units.contains(id);
    }

    bool HasUnitConfig(const std::string &id) const
    {
        const auto [units, lock] = unitConfig_.BeginRead();
        return units.contains(id);
    }

    bool HasTemplate(const std::string &id) const
    {
        const auto [templates, lock] = templates_.BeginRead();
        return templates.contains(id);
    }

    bool HasTemplateConfig(const std::string &id) const
    {
        const auto [templates, lock] = templateConfig_.BeginRead();
        return templates.contains(id);
    }

    std::map<std::string, std::shared_ptr<IUnit>> GetAllUnits() const
    {
        return units_.Clone();
    }

    std::map<std::string, std::shared_ptr<Template>> GetAllTemplates() const
    {
        return templates_.Clone();
    }

    std::shared_ptr<IUnit> GetUnit(const std::string &id) const
    {
        const auto [units, lock] = units_.BeginRead();
        if (units.contains(id))
            return units.at(id);
        return nullptr;
    }

    template<typename TUnit>
    requires(std::derived_from<TUnit, Unit> && !std::is_same_v<TUnit, Unit>) std::shared_ptr<TUnit> GetUnit(const std::string &id) const
    {
        const auto [units, lock] = units_.BeginRead();
        if (units.contains(id))
            return std::dynamic_pointer_cast<TUnit>(units.at(id));
        return nullptr;
    }

    std::shared_ptr<Template> GetTemplate(const std::string &id) const
    {
        const auto [templates, lock] = templates_.BeginRead();
        if (templates.contains(id))
            return templates.at(id);
        return nullptr;
    }

    std::optional<toml::table> GetTemplateConfig(const std::string &id) const
    {
        const auto [templates, lock] = templateConfig_.BeginRead();
        if (templates.contains(id))
            return templates.at(id);
        return std::nullopt;
    }

    std::map<std::pair<std::string, ArgumentMap>, std::string> GetTemplateOverrides() const
    {
        return templateOverrides_.Clone();
    }

    std::vector<std::pair<std::string, ArgumentMap>> LookupTemplate(const std::string &id, const ArgumentMap &args);

    std::shared_ptr<IUnit> InstantiateUnit(const std::string &template_id, const ArgumentMap &parameters);

    void AddUnit(std::shared_ptr<IUnit> unit);

  private:
    [[nodiscard]] bool FillInheritanceRecursive(const std::string &inherits, toml::table &table, ArgumentMap &args) const;
    std::shared_ptr<Template> DoCreateTemplate(const std::string &id) const;

  private:
    std::string defaultTarget;

    Locked<std::map<std::string, toml::table>> templateConfig_;
    Locked<std::map<std::string, toml::table>> unitConfig_;

    Locked<std::map<std::string, std::shared_ptr<Template>>> templates_;
    Locked<std::map<std::string, std::shared_ptr<IUnit>>> units_;

    Locked<std::map<std::pair<std::string, ArgumentMap>, std::string>> templateOverrides_;
};

const inline std::unique_ptr<ConfigurationManagerImpl> ConfigurationManager = std::make_unique<ConfigurationManagerImpl>();
