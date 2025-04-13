// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <iostream>
#include <toml++/toml.hpp>

struct Unit;
using ArgumentMap = std::map<std::string, std::string>;

static constexpr std::string_view TEMPLATE_SUFFIX = "-template";

class Template : public std::enable_shared_from_this<Template>
{
  public:
    explicit Template(const std::string &id, const toml::table &table);

    const std::string id;
    const toml::table table;

    std::vector<std::string> parameters;

    std::optional<std::pair<std::string, std::shared_ptr<Unit>>> Instantiate(const ArgumentMap &args) const;

  private:
    std::string GetID(const ArgumentMap &args) const;
};
