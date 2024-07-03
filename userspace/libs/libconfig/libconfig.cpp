// SPDX-License-Identifier: GPL-3.0-or-later

#include "libconfig/libconfig.h"

#include <fstream>
#include <iostream>

std::optional<Config> Config::from_file(const std::string &filepath)
{
    std::fstream f(filepath, f.in);
    if (!f)
        return std::nullopt;

    Config config;

    Config::SectionType current_section = std::make_pair("global", Config::SectionContentType{});
    for (std::string line; std::getline(f, line);)
    {
        if (line.empty())
            continue;

        if (line.starts_with('#'))
            continue;

        if (line.starts_with('[') && line.ends_with(']'))
        {
            // a new section
            config.sections.push_back(current_section);
            current_section = std::make_pair(line.substr(1, line.size() - 2), Config::SectionContentType{}); // remove the brackets
        }
        else
        {
            // a key-value pair
            size_t pos = line.find('=');
            if (pos == std::string::npos)
            {
                std::cerr << "Invalid line: " << line << std::endl;
                continue;
            }

            std::string key = line.substr(0, pos);
            std::string value = line.substr(pos + 1);

            const auto remove_leading_trailing_spaces = [](std::string &str)
            {
                while (!str.empty() && str.front() == ' ')
                    str.erase(0, 1);
                while (!str.empty() && str.back() == ' ')
                    str.pop_back();
            };

            remove_leading_trailing_spaces(key);
            remove_leading_trailing_spaces(value);

            current_section.second.push_back({ key, value });
        }
    }

    config.sections.push_back(current_section);

    return config;
}

std::optional<Config::SectionType> Config::get_section(const std::string &section_name) const
{
    for (const auto &section : sections)
        if (section.first == section_name)
            return section;

    return std::nullopt;
}

std::vector<Config::EntryType> Config::get_entry(const std::string &section_name, const std::string &key) const
{
    auto entries = get_section(section_name);
    if (!entries)
        return {};
    return entries->second | std::ranges::views::filter([&key](const auto &entry) { return entry.first == key; }) | std::ranges::to<std::vector<EntryType>>();
}

std::vector<Config::EntryType> Config::get_entries(const std::string &section_name) const
{
    auto entries = get_section(section_name);
    if (!entries)
        return {};
    return entries->second;
}
