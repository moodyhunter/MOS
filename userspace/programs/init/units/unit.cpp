// SPDX-License-Identifier: GPL-3.0-or-later
#include "units/unit.hpp"

#include "global.hpp"
#include "logging.hpp"

#include <vector>

const auto INVALID_ARGUMENT = "<invalid>";

static bool is_optional_key(const std::string_view key)
{
    return key == "depends_on" || key == "part_of";
}

const std::map<std::string, UnitCreatorType> &Unit::Creator(std::optional<std::pair<std::string, UnitCreatorType>> creator)
{
    static std::map<std::string, UnitCreatorType> creators;
    if (creator)
        creators.insert(*creator);
    return creators;
}

const std::map<std::string, UnitInstantiator> &Unit::Instantiator(std::optional<std::pair<std::string, UnitInstantiator>> instantiator)
{
    static std::map<std::string, UnitInstantiator> instantiators;
    if (instantiator)
        instantiators.insert(*instantiator);
    return instantiators;
}

void Unit::VerifyUnitArguments(const std::string &id, const toml::table &table)
{
    if (table.empty())
        return;

    for (const auto &[key, value] : table)
    {
        if (key == "options")
        {
            if (!value.is_table())
            {
                std::cerr << "unit " << id << " has bad options" << std::endl;
                continue;
            }

            for (const auto &[key, value] : *value.as_table())
                std::cerr << "unit " << id << " has unknown options:  " << key << std::endl;
        }
        else
        {
            std::cerr << "unit " << id << " has unknown keys:  " << key << std::endl;
        }
    }
}

std::shared_ptr<IUnit> Unit::Create(const std::string &id, const toml::table &data)
{
    // extract type from id, e.g. mos.service@abc -> service
    const auto type_string = id.substr(id.find('.') + 1, id.find('@') - id.find('.') - 1);
    if (type_string.empty())
    {
        std::cerr << "bad unit id" << std::endl;
        return nullptr;
    }

    const auto &creators = Unit::Creator();
    if (const auto it = creators.find(type_string); it != creators.end())
    {
        Debug << "creating unit " << id << " of type " << type_string << std::endl;
        auto data_copy = data;
        if (const auto unit = it->second(id, data_copy); unit)
        {
            VerifyUnitArguments(id, data_copy);
            return unit;
        }

        std::cerr << "failed to create unit" << std::endl;
        return nullptr;
    }
    else
    {
        std::cerr << RED("unknown type ") << type_string << std::endl;
        return nullptr;
    }
}

std::shared_ptr<IUnit> Unit::Instantiate(const std::string &id, std::shared_ptr<const Template> template_, const ArgumentMap &args)
{
    const auto &instantiators = Unit::Instantiator();

    const auto type_string = id.substr(id.find('.') + 1, id.find('@') - id.find('.') - 1);
    if (type_string.empty())
    {
        std::cerr << "bad unit id" << std::endl;
        return nullptr;
    }

    if (const auto it = instantiators.find(type_string); it != instantiators.end())
    {
        Debug << "instantiating unit " << id << " of type " << type_string << std::endl;
        if (const auto unit = it->second(id, template_, args); unit)
            return unit;

        std::cerr << "failed to instantiate unit" << std::endl;
        return nullptr;
    }

    std::cerr << RED("unknown type ") << type_string << std::endl;
    return nullptr;
}

std::ostream &operator<<(std::ostream &os, const Unit &unit)
{
    os << unit.description << " (" << unit.id << ")" << std::endl;
    os << "  depends_on: ";
    for (const auto &dep : unit.dependsOn)
        os << dep << " ";
    if (unit.dependsOn.empty())
        os << "(none)";
    os << std::endl;
    os << "  part_of: ";
    for (const auto &part : unit.partOf)
        os << part << " ";
    if (unit.partOf.empty())
        os << "(none)";
    os << std::endl;
    unit.onPrint(os);
    return os;
}

Unit::Unit(const std::string &id, toml::table &table, std::shared_ptr<const Template> template_, const ArgumentMap &args)
    : IUnit(id),                                             //
      arguments(args),                                       //
      description(PopArg(table, "description", toplevel)),   //
      dependsOn(GetArrayArg(table, "depends_on", toplevel)), //
      partOf(GetArrayArg(table, "part_of", toplevel)),       //
      template_(template_)
{
    Debug << "creating unit " << id << std::endl;
}

std::string Unit::PopArg(toml::table &table, std::string_view key)
{
    if (!table["options"].is_table())
        return INVALID_ARGUMENT;
    return PopArg(*table["options"].as_table(), key, toplevel);
}

std::vector<std::string> Unit::GetArrayArg(toml::table &table, std::string_view key)
{
    if (!table["options"].is_table())
        return {};
    return GetArrayArg(*table["options"].as_table(), key, toplevel);
}

std::string Unit::PopArg(toml::table &table, std::string_view key, toplevel_t)
{
    const auto tomlval = table[key];
    if (!tomlval || !tomlval.is_string())
    {
        if (is_optional_key(key))
            return "";
        std::cerr << "unit " << id << " missing key " << key << std::endl;
        return INVALID_ARGUMENT;
    }

    const auto value = ReplaceArgs(tomlval.as_string()->get());
    table.erase(key);
    return value;
}

std::vector<std::string> Unit::GetArrayArg(toml::table &table, std::string_view key, toplevel_t)
{
    const auto tomlval = table[key];
    if (!tomlval || !tomlval.is_array())
    {
        if (is_optional_key(key))
            return {};
        std::cerr << "unit " << id << " missing key " << key << std::endl;
        return {};
    }

    const auto value = ReplaceArgs(tomlval.as_array());
    table.erase(key);
    return value;
}

std::string Unit::ReplaceArgs(const std::string &str) const
{
    // replace any $key with value in args
    std::string result = str;
    for (const auto &[key, value] : arguments)
        result = replace_all(result, "[" + key + "]", value);
    return result;
}

std::vector<std::string> Unit::ReplaceArgs(const toml::array *array)
{
    std::vector<std::string> result;
    for (const auto &e : *array)
    {
        if (!e.is_string())
        {
            std::cerr << "Invalid array element" << std::endl;
            continue;
        }
        result.push_back(ReplaceArgs(e.as_string()->get()));
    }
    return result;
}
