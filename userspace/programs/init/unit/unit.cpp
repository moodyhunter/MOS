// SPDX-License-Identifier: GPL-3.0-or-later
#include "unit/unit.hpp"

#include "global.hpp"
#include "logging.hpp"

#include <vector>

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

    const auto unit = creators.at(type_string)(id, *data);
    if (!unit)
    {
        std::cerr << "failed to create unit" << std::endl;
        return nullptr;
    }

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

Unit::Unit(const std::string &id, const toml::table &table, std::shared_ptr<const Template> template_, const ArgumentMap &args)
    : id(id),                                                             //
      arguments(args),                                                    //
      description(ReplaceArgs(table["description"].value_or("unknown"))), //
      template_(template_)
{
    table["depends_on"].visit(VectorAppender(this->dependsOn));
    table["part_of"].visit(VectorAppender(this->partOf));

    if (this->template_)
    {
        if (const auto template_args = template_->table["template_args"]; template_args)
            template_args.visit(VectorAppender(this->templateArgs));

        // verify that all template_args are present in args
        VerifyArguments(this->templateArgs, args);

        for (auto &dep : dependsOn)
            dep = ReplaceArgs(dep);

        for (auto &part : partOf)
            part = ReplaceArgs(part);
    }
}
