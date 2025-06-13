// SPDX-License-Identifier: GPL-3.0-or-later

#include "units/template.hpp"

#include "global.hpp"
#include "logging.hpp"
#include "units/unit.hpp"

static inline constexpr auto VectorAppender(std::vector<std::string> &target)
{
    return [&](const toml::array &array)
    {
        for (const auto &dep : array)
            target.push_back(**dep.as_string());
    };
};

static bool VerifyArguments(const std::vector<std::string> &params, const ArgumentMap &args)
{
    const auto &rhs = args;
    const auto b1 = std::all_of(params.begin(), params.end(), [&rhs](const auto &p) { return rhs.contains(p); });
    const auto b2 = std::all_of(rhs.begin(), rhs.end(), [&params](const auto &p) { return std::find(params.begin(), params.end(), p.first) != params.end(); });

    if (!b1)
        std::cerr << RED("Missing required arguments for unit instantiation.") << std::endl;

    if (!b2)
        Debug << RED("Extraneous arguments for unit instantiation.") << std::endl;

    return b1 && b2;
}

Template::Template(const std::string &id, const toml::table &table, const ArgumentMap &predefined_args) : id(id), table(table), predefined_args(predefined_args)
{
    if (const auto template_params = table["template_params"]; template_params)
        template_params.visit(VectorAppender(parameters));
    else
        std::cerr << "template " << id << " missing template_params" << std::endl;
}

std::optional<std::pair<std::string, std::shared_ptr<IUnit>>> Template::Instantiate(const ArgumentMap &args) const
{
    ArgumentMap args_copy = predefined_args;
    for (const auto &[key, value] : args)
        args_copy[key] = value;

    if (!VerifyArguments(parameters, args_copy))
        return std::nullopt;

    std::vector<std::string> templateArgs;
    table["template_params"].as_array()->visit(VectorAppender(templateArgs));

    if (templateArgs.empty() || args_copy.empty() || !VerifyArguments(templateArgs, args_copy))
    {
        std::cerr << "template " << id << " has incorrect arguments" << std::endl;
        return std::nullopt;
    }

    const auto new_unit_id = GetID(id, args); // don't involve predefined_args
    return std::make_pair(new_unit_id, Unit::Instantiate(new_unit_id, shared_from_this(), args_copy));
}

std::string Template::GetID(const std::string &id, const ArgumentMap &args)
{
    std::ostringstream ss;
    ss << id.substr(0, id.find(TEMPLATE_SUFFIX)) << ARGUMENTS_SEPARATOR;

    for (auto it = args.begin(); it != args.end(); it++)
    {
        ss << it->first << "=" << it->second;
        if (std::next(it) != args.end())
            ss << ',';
    }

    return ss.str();
}
