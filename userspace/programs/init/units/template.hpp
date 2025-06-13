// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <toml++/toml.hpp>
#include <vector>

struct IUnit;
using ArgumentMap = std::map<std::string, std::string>;

static constexpr std::string_view TEMPLATE_SUFFIX = "-template";
constexpr auto ARGUMENTS_SEPARATOR = '@';

class Template : public std::enable_shared_from_this<Template>
{
  public:
    explicit Template(const std::string &id, const toml::table &table, const ArgumentMap &predefined_args = {});

    const std::string id;
    const toml::table table;
    const ArgumentMap predefined_args;

    std::optional<std::pair<std::string, std::shared_ptr<IUnit>>> Instantiate(const ArgumentMap &args) const;

    std::vector<std::string> GetParameters() const
    {
        // return key in parameters but not in predefined_args
        std::vector<std::string> result;
        for (const auto &param : parameters)
            if (!predefined_args.contains(param))
                result.push_back(param);
        return result;
    }

    static std::string GetID(const std::string &id, const ArgumentMap &args);

  private:
    std::vector<std::string> parameters;
};
