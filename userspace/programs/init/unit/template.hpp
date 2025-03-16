// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <iostream>
#include <toml++/toml.hpp>

struct Unit;
using ArgumentMap = std::map<std::string, std::string>;

static inline constexpr auto VectorAppender(std::vector<std::string> &target)
{
    return [&](const toml::array &array)
    {
        for (const auto &dep : array)
            target.push_back(**dep.as_string());
    };
};

bool VerifyArguments(const std::vector<std::string> &params, const ArgumentMap &args);

struct Template : public std::enable_shared_from_this<Template>
{
    explicit Template(const std::string &id, const toml::table &table) : id(id), table(table)
    {
        if (const auto template_args = table["template_args"]; template_args)
            template_args.visit(VectorAppender(parameters));
        else
            std::cerr << "template " << id << " missing template_args" << std::endl;
    }

    const std::string id;
    const toml::table table;

    std::vector<std::string> parameters;

    std::shared_ptr<Unit> Instantiate(const ArgumentMap &args) const;
};
