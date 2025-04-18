// SPDX-License-Identifier: GPL-3.0-or-later

#include "unit/template.hpp"

#include "logging.hpp"
#include "unit/unit.hpp"

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
        Debug << "Missing required arguments for unit instantiation." << std::endl;

    if (!b2)
        Debug << "Extraneous arguments for unit instantiation." << std::endl;

    return b1 && b2;
}

Template::Template(const std::string &id, const toml::table &table) : id(id), table(table)
{
    if (const auto template_args = table["template_args"]; template_args)
        template_args.visit(VectorAppender(parameters));
    else
        std::cerr << "template " << id << " missing template_args" << std::endl;
}

std::optional<std::pair<std::string, std::shared_ptr<Unit>>> Template::Instantiate(const ArgumentMap &args) const
{
    if (!VerifyArguments(parameters, args))
        return std::nullopt;

    std::vector<std::string> templateArgs;
    table["template_args"].as_array()->visit(VectorAppender(templateArgs));

    if (templateArgs.empty() || args.empty() || !VerifyArguments(templateArgs, args))
    {
        std::cerr << "template " << id << " has incorrect arguments" << std::endl;
        return std::nullopt;
    }

    const auto new_unit_id = GetID(args);
    return std::make_pair(new_unit_id, Unit::CreateFromTemplate(new_unit_id, shared_from_this(), args));
}

std::string Template::GetID(const ArgumentMap &args) const
{
    std::ostringstream ss;
    ss << id.substr(0, id.find(TEMPLATE_SUFFIX)) << '@';

    for (auto it = args.begin(); it != args.end(); it++)
    {
        ss << it->second;
        if (std::next(it) != args.end())
            ss << '-';
    }

    return ss.str();
}
