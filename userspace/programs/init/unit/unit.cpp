// SPDX-License-Identifier: GPL-3.0-or-later
#include "unit/unit.hpp"

#include "global.hpp"
#include "logging.hpp"

#include <vector>

const auto INVALID_ARGUMENT = "<invalid>";

static bool is_optional_key(const std::string_view key)
{
    return key == "depends_on" || key == "part_of";
}

std::map<std::string, UnitCreatorType> &Unit::Creator(std::optional<std::pair<std::string, UnitCreatorType>> creator)
{
    static std::map<std::string, UnitCreatorType> creators;
    if (creator)
        creators.insert(*creator);
    return creators;
}

std::map<std::string, UnitInstantiator> &Unit::Instantiator(std::optional<std::pair<std::string, UnitInstantiator>> instantiator)
{
    static std::map<std::string, UnitInstantiator> instantiators;
    if (instantiator)
        instantiators.insert(*instantiator);
    return instantiators;
}

void Unit::VerifyUnitArguments(const std::string &id, toml::table &table)
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

std::shared_ptr<Unit> Unit::CreateNew(const std::string &id, const toml::table *data)
{
    const auto &creators = Unit::Creator();

    const auto type = (*data)["type"];
    if (!type.is_string())
    {
        std::cerr << "bad type, expect string" << std::endl;
        return nullptr;
    }

    const auto type_string = type.as_string()->get();
    if (!creators.contains(type_string))
    {
        std::cerr << RED("unknown type ") << type_string << std::endl;
        return nullptr;
    }

    auto table = *data;
    const auto unit = creators.at(type_string)(id, table);
    if (!unit)
    {
        std::cerr << "failed to create unit" << std::endl;
        return nullptr;
    }

    VerifyUnitArguments(id, table);

    return unit;
}

std::shared_ptr<Unit> Unit::CreateFromTemplate(const std::string &id, std::shared_ptr<const Template> template_, const ArgumentMap &args)
{
    const auto &instantiators = Unit::Instantiator();

    const auto type = template_->table["type"];
    if (!type.is_string())
    {
        std::cerr << "bad type, expect string" << std::endl;
        return nullptr;
    }

    const auto type_string = type.as_string()->get();
    if (!instantiators.contains(type_string))
    {
        std::cerr << RED("unknown type ") << type_string << std::endl;
        return nullptr;
    }

    Debug << "instantiating unit " << id << " of type " << type_string << std::endl;
    const auto unit = instantiators.at(type_string)(id, template_, args);
    if (!unit)
    {
        std::cerr << "failed to instantiate unit" << std::endl;
        return nullptr;
    }

    return unit;
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
    : arguments(args),                                       //
      id(id),                                                //
      description(GetArg(table, "description", toplevel)),   //
      dependsOn(GetArrayArg(table, "depends_on", toplevel)), //
      partOf(GetArrayArg(table, "part_of", toplevel)),       //
      template_(template_)
{
    table.erase("type");
}

std::string Unit::GetArg(toml::table &table, std::string_view key)
{
    return GetArg(*table["options"].as_table(), key, toplevel);
}

std::vector<std::string> Unit::GetArrayArg(toml::table &table, std::string_view key)
{
    return GetArrayArg(*table["options"].as_table(), key, toplevel);
}

std::string Unit::GetArg(toml::table &table, std::string_view key, toplevel_t)
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
