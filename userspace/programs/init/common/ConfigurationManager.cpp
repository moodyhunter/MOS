// SPDX-License-Identifier: GPL-3.0-or-later
#include "common/ConfigurationManager.hpp"

#include "global.hpp"
#include "logging.hpp"
#include "units/inherited.hpp"
#include "units/template.hpp"

#include <algorithm>
#include <iostream>
#include <iterator>
#include <memory>
#include <set>
#include <string>

void ConfigurationManagerImpl::LoadConfiguration(std::vector<toml::table> &&tables_)
{
    auto tables = tables_;
    {
        auto &main = tables.front();
        defaultTarget = main["default_target"].value_or("normal.target");
        main.erase("default_target");
    }

    for (const auto &table : tables)
    {
        for (const auto &[key, value] : table)
        {
            if (!value.is_table())
            {
                std::cerr << RED("bad table") << " " << key << std::endl;
                continue;
            }

            for (const auto &[subkey, subvalue] : *value.as_table())
            {
                const auto id = key.data() + "."s + subkey.data();
                if (HasUnit(id))
                {
                    std::cerr << "unit " << id << " " RED("already exists") << std::endl;
                    continue;
                }

                const auto data = subvalue.as_table();

                if (!data || !data->is_table() || data->empty())
                {
                    std::cerr << RED("bad unit, expect table") << std::endl;
                    continue;
                }

                if (id.ends_with(TEMPLATE_SUFFIX))
                {
                    const auto [templates, lock] = templateConfig_.BeginWrite();
                    templates[id] = *data;
                    Debug << "loaded template " << id << std::endl;
                }
                else
                {
                    const auto [units, lock] = unitConfig_.BeginWrite();
                    units[id] = *data;
                    Debug << "created unit " << id << std::endl;
                }
            }
        }
    }
}

static void FillArguments(const toml::table *arg_table, ArgumentMap &out_args)
{
    // remove corresponding template_params from out_table
    if (!arg_table)
    {
        std::cerr << RED("inherits_args isn't a valid table") << std::endl;
        return;
    }

    for (const auto &[arg_key, arg_value] : *arg_table)
    {
        if (arg_value.is_string())
            out_args[arg_key.data()] = arg_value.as_string()->get();
        else
            std::cerr << RED("bad template args") << " " << arg_key << std::endl;
    }
}

static void MergeTemplateTable(const toml::table &template_table, toml::table &out_table, ArgumentMap &out_args)
{
    for (const auto &[in_key, in_value] : template_table)
    {
        // don't merge these keys
        if (in_key == "inherits")
            continue;
        else if (in_key == "inherits_args")
        {
            FillArguments(in_value.as_table(), out_args);
            continue;
        }
        else if (in_key == "template_params")
        {
            const auto in_array = in_value.as_array();
            if (!in_array)
            {
                std::cerr << RED("template_params isn't a valid array") << std::endl;
                continue;
            }

            std::set<std::string> keys;

            // append incoming template_params to keys
            for (const auto &e : *in_array)
            {
                if (!e.is_string())
                {
                    std::cerr << RED("template_params isn't a valid string") << std::endl;
                    continue;
                }
                keys.insert(e.as_string()->get());
            }

            // add existing template_params to keys
            const auto existing = out_table["template_params"].as_array();
            if (existing)
            {
                for (const auto &e : *existing)
                {
                    if (!e.is_string())
                    {
                        std::cerr << RED("template_params isn't a valid string") << std::endl;
                        continue;
                    }
                    keys.insert(e.as_string()->get());
                }
            }
            // create new array
            auto new_array = toml::array();
            for (const auto &key : keys)
                new_array.push_back(key);
            out_table.emplace("template_params", std::move(new_array));
        }
        else
        {
            // directly replace the value
            out_table.insert_or_assign(in_key.str(), in_value);
        }
    }
}

bool ConfigurationManagerImpl::FillInheritanceRecursive(const std::string &inherits, toml::table &out_table, ArgumentMap &out_args) const
{
    const auto templateConfig = GetTemplateConfig(inherits);
    if (!templateConfig)
    {
        std::cerr << RED("template not found") << " " << inherits << std::endl;
        return false;
    }

    if (!templateConfig->contains("inherits"))
    {
        // this template no longer inherits others, safe to fill current configuration to output
        out_table = *templateConfig;
        MergeTemplateTable(*templateConfig, out_table, out_args);
        out_table.erase("inherits");
        out_table.erase("inherits_args");
        return true;
    }

    if (!FillInheritanceRecursive(**templateConfig->at("inherits").as_string(), out_table, out_args))
        return false;

    // now merge templateConfig with out_table
    MergeTemplateTable(*templateConfig, out_table, out_args);
    return true;
}

std::shared_ptr<Template> ConfigurationManagerImpl::DoCreateTemplate(const std::string &id) const
{
    if (HasTemplate(id))
    {
        std::cerr << RED("template already exists") << " " << id << std::endl;
        return nullptr;
    }

    const auto templateConfig = GetTemplateConfig(id);
    if (!templateConfig)
    {
        std::cerr << RED("template not found") << " " << id << std::endl;
        return nullptr;
    }

    toml::table table;
    ArgumentMap args;

    if (const auto it = templateConfig->find("inherits"); it != templateConfig->end())
    {
        if (!FillInheritanceRecursive(id, table, args))
            return nullptr;
    }
    else
    {
        // this template doesn't inherit anything, just copy the table
        table = *templateConfig;
    }

    if (const auto inherits_args = templateConfig->find("inherits_args"); inherits_args != templateConfig->end())
    {
        if (inherits_args->second.is_table())
            FillArguments(inherits_args->second.as_table(), args);
        else
            std::cerr << RED("inherits_args isn't a valid table") << std::endl;
    }
    return std::make_shared<Template>(id, table, args);
}

void ConfigurationManagerImpl::FinaliseConfiguration()
{
    // create all templates
    {
        const auto [templateConfig, lock] = templateConfig_.BeginRead();
        const auto [templateOverrides, lock2] = templateOverrides_.BeginWrite();

        std::set<std::string> leafTemplates;

        for (const auto &[id, table] : templateConfig)
        {
            const auto it = table.find("inherits");
            if (it == table.end())
                continue;

            const auto &value = it->second;
            if (!value.is_string())
            {
                std::cout << RED("bad table: ") << id << std::endl;
                continue;
            }

            ArgumentMap inheritArgs;
            if (const auto it = table.find("inherits_args"); it != table.end())
            {
                if (it->second.is_table())
                    FillArguments(it->second.as_table(), inheritArgs);
                else
                    std::cerr << RED("inherits_args isn't a valid table") << std::endl;
            }

            const auto key = std::make_pair(**value.as_string(), inheritArgs);
            templateOverrides.insert_or_assign(key, id);
            leafTemplates.emplace(id);
        }

        for (const auto &[leaf, _] : templateConfig)
        {
            if (HasTemplate(leaf))
                __builtin_unreachable();

            const auto template_ = DoCreateTemplate(leaf);
            if (!template_)
                continue;

            const auto [templates, lock] = templates_.BeginWrite();
            templates[leaf] = template_;
        }
    }

    // create all units
    {
        const auto [unitConfig, lock] = unitConfig_.BeginRead();
        for (const auto &[id, data] : unitConfig)
        {
            if (HasUnit(id))
                continue;

            const auto unit = Unit::Create(id, data);
            if (!unit)
            {
                std::cerr << RED("failed to create unit") << " " << id << std::endl;
                continue;
            }

            const auto [units, lock] = units_.BeginWrite();
            units[id] = unit;
        }
    }

    // organise dependencies
    {
        const auto [units, lock] = units_.BeginRead();
        for (const auto &[id, unit] : units)
        {
            for (const auto &partid : unit->GetPartOf())
            {
                if (!units.contains(partid))
                {
                    std::cerr << "unit " << unit->id << " is part of non-existent unit " << partid << std::endl;
                    continue;
                }
                const auto target = units.at(partid);
                if (target->GetType() != UnitType::Target)
                {
                    std::cerr << "unit " << unit->id << " is part of non-target unit " << partid << std::endl;
                    continue;
                }

                // target->AddDependency(unit->id);
                const auto targetUnit = std::dynamic_pointer_cast<Target>(target);
                if (!targetUnit)
                {
                    std::cerr << "unit " << partid << " is not a target" << std::endl;
                    continue;
                }
                targetUnit->AddMember(unit->id);
                Debug << "unit " << unit->id << " is now part of target " << partid << std::endl;
            }
        }
    }
}

std::vector<std::pair<std::string, ArgumentMap>> ConfigurationManagerImpl::LookupTemplate(const std::string &id, const ArgumentMap &args)
{
    std::vector<std::pair<std::string, ArgumentMap>> intermediateTemplates{ { id, args } };

    {
        const auto [templateOverrides, lock] = templateOverrides_.BeginRead();
        std::string currentId = id;
        ArgumentMap currentArgs = args;
        while (true)
        {
            bool found = false;
            for (const auto &[pair, overridden] : templateOverrides)
            {
                if (pair.first != currentId)
                    continue;

                // if all pair.second keys and values are in args, use that template
                auto argsCopy = currentArgs;

                const auto argsAreValid = [&](const auto &pair)
                {
                    const auto &[key, value] = pair;
                    if (!argsCopy.contains(key) || argsCopy.at(key) != value)
                        return false;
                    argsCopy.erase(key); // already searched
                    return true;
                };

                found = std::all_of(pair.second.begin(), pair.second.end(), argsAreValid);
                if (found)
                {
                    currentId = overridden;
                    currentArgs = argsCopy;
                    intermediateTemplates.emplace_back(currentId, currentArgs);
                    break;
                }
            }

            if (!found)
                break;
        }
    }

    int level = 0;
    for (const auto &[id, args] : intermediateTemplates)
    {
        // print the template tree
        Debug << std::string(level * 2, ' ') << WHITE(+id +) << ", ";
        if (args.empty())
            Debug << BLUE("no args");
        else
        {
            Debug << BLUE("args: ");
            for (const auto &[key, value] : args)
                Debug << YELLOW(+key +) << " = " << GREEN(+value +) << ", ";
        }
        Debug << std::endl;
        level++;
    }

    return intermediateTemplates;
}

std::shared_ptr<IUnit> ConfigurationManagerImpl::InstantiateUnit(const std::string &template_id, const ArgumentMap &parameters)
{
    const auto intermediateTemplates = LookupTemplate(template_id, parameters);
    if (intermediateTemplates.empty())
        return nullptr;

    // use the last template in the chain
    const auto &[found_template_id, found_template_args] = intermediateTemplates.back();

    const auto template_ = GetTemplate(found_template_id);
    if (!template_)
        return nullptr;

    const auto result = template_->Instantiate(found_template_args);
    if (!result)
        return nullptr;

    const auto [units, lock] = units_.BeginWrite();
    const auto [unit_id, unit] = *result;
    if (units.contains(unit_id))
        return nullptr;

    units[unit_id] = unit;

    if (intermediateTemplates.size() > 1)
    {
        std::shared_ptr<IUnit> childUnit = unit;
        for (auto it = intermediateTemplates.rbegin() + 1; it != intermediateTemplates.rend(); ++it)
        {
            const auto newId = Template::GetID(it->first, it->second);
            auto inheritedUnit = std::make_shared<InheritedUnit>(newId, childUnit);
            if (units.contains(newId))
            {
                std::cerr << RED("inherited unit already exists") << " " << newId << std::endl;
                continue;
            }
            units[newId] = inheritedUnit;
            childUnit = inheritedUnit;
        }
    }

    return unit;
}

void ConfigurationManagerImpl::AddUnit(std::shared_ptr<IUnit> unit)
{
    if (!unit)
        return;

    const auto [units, lock] = units_.BeginWrite();
    if (units.contains(unit->id))
    {
        std::cerr << RED("unit already exists") << " " << unit->id << std::endl;
        return;
    }
    units[unit->id] = unit;
}
